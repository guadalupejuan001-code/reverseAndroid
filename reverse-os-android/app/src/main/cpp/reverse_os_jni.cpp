#include "session.hpp"
#include "rxe.hpp"
#include "rxe_app.hpp"
#include "minimal_guest_demo.hpp"
#include "minimal_guest_system_demo.hpp"
#include "minimal_guest_fp_demo.hpp"
#include "guest_file_session.hpp"
#include "guest_resident_session.hpp"
#include "minimal_guest_pci_demo.hpp"
#include "minimal_guest_counter_demo.hpp"
#include <jni.h>
#include <exception>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
reverse_pc::guest::GuestResidentRegistry& guest_sessions(){static reverse_pc::guest::GuestResidentRegistry registry;return registry;}
std::shared_ptr<const reverse_pc::guest::GuestBlockImage> guest_disk(const std::string& path){
  static std::mutex mutex;static std::string cached_path;
  static std::shared_ptr<const reverse_pc::guest::GuestBlockImage> cached;
  std::lock_guard<std::mutex> guard(mutex);
  if(path!=cached_path){auto next=path.empty()?nullptr:reverse_pc::guest::GuestBlockImage::open(path);cached=std::move(next);cached_path=path;}
  return cached;
}
std::vector<std::uint8_t> bytes(JNIEnv* e,jbyteArray value) {
  if(!value)return {};
  const jsize n=e->GetArrayLength(value);
  if(n<0||std::size_t(n)>reverse_os::max_state+29)
    throw std::length_error("Android state exceeds transport budget");
  std::vector<std::uint8_t> out(static_cast<std::size_t>(n));
  if(n)e->GetByteArrayRegion(value,0,n,reinterpret_cast<jbyte*>(out.data()));
  if(e->ExceptionCheck())throw std::runtime_error("JNI state read failed");
  return out;
}
jbyteArray output(JNIEnv* e,const std::vector<std::uint8_t>& v){
  if(v.size()>std::size_t(std::numeric_limits<jsize>::max()))throw std::length_error("JNI result too long");
  jbyteArray arr=e->NewByteArray(jsize(v.size()));
  if(arr&&!v.empty())e->SetByteArrayRegion(arr,0,jsize(v.size()),reinterpret_cast<const jbyte*>(v.data()));
  return arr;
}
std::string source(JNIEnv* e,jstring text){
  if(!text)throw std::invalid_argument("XI program missing");
  const char* chars=e->GetStringUTFChars(text,nullptr);
  if(!chars)throw std::runtime_error("JNI string unavailable");
  std::string s(chars);e->ReleaseStringUTFChars(text,chars);
  if(s.size()>16*1024*1024)throw std::length_error("XI program too large");
  return s;
}
void throw_java(JNIEnv* e,const std::exception& err){
  if(e->ExceptionCheck())return;
  auto cls=e->FindClass("java/lang/IllegalStateException");
  if(cls)e->ThrowNew(cls,err.what());
}
std::vector<std::uint8_t> guest_packet(const reverse_pc::guest::MinimalGuest& guest,
                                     const reverse_pc::guest::RunResult* run=nullptr,const std::string& xi_output={}){
  const auto state=guest.snapshot();
  if(state.size()>reverse_os::max_state)throw std::length_error("Guest exceeds Android checkpoint transport budget");
  std::ostringstream text;text<<"{\"guest\":"<<guest.statistics();
  if(run)text<<",\"executed_this_run\":"<<run->executed<<",\"stdout\":"<<reverse_pc::guest::quote(run->output)
    <<",\"stderr\":"<<reverse_pc::guest::quote(run->error_output);
  if(!xi_output.empty())text<<",\"xi_output\":"<<reverse_pc::guest::quote(xi_output);
  text<<",\"snapshot_bytes\":"<<state.size()<<"}";
  return reverse_os::packet(text.str(),state);
}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_initialize(JNIEnv* e,jclass){
  try{return output(e,reverse_os::save(reverse_os::Runtime{}));}
  catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_execute(JNIEnv* e,jclass,jbyteArray state,jstring text){
  try{
    auto runtime=reverse_os::restore(bytes(e,state));
    const auto lines=runtime.execute(source(e,text));
    return output(e,reverse_os::packet(lines,reverse_os::save(runtime)));
  }catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_reversepc_os_NativeRuntime_inspect(JNIEnv* e,jclass,jbyteArray state){
  try{
    auto runtime=reverse_os::restore(bytes(e,state));
    const auto q=runtime.core().stats();
    const auto m=runtime.memory().stats();
    std::ostringstream s;
    s<<"{\"active_qubits\":"<<q.active_qubits
     <<",\"components\":"<<q.components
     <<",\"largest_component\":"<<q.largest_component
     <<",\"tableau_bytes\":"<<q.tableau_bytes
     <<",\"ether_rules\":"<<q.ether_rules
     <<",\"core_revision\":"<<q.revision
     <<",\"memory_revision\":"<<m.revision
     <<",\"memory_overrides\":"<<m.overrides
     <<",\"quantum_bindings\":"<<m.quantum_bindings
     <<",\"state_format\":\"RVPC2\"}";
    return e->NewStringUTF(s.str().c_str());
  }catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_compileRxe(JNIEnv* e,jclass,jbyteArray program,jbyteArray title){
  try{const auto p=bytes(e,program),t=bytes(e,title);return output(e,reverse_pc::rxe::compile(std::string(p.begin(),p.end()),std::string(t.begin(),t.end())));}
  catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_inspectRxe(JNIEnv* e,jclass,jbyteArray program){
  try{const auto image=bytes(e,program);const auto description=reverse_pc::rxe_app::has_magic(image)?reverse_pc::rxe_app::describe(image):reverse_pc::rxe::describe(image);return output(e,std::vector<std::uint8_t>(description.begin(),description.end()));}
  catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_executeRxe(JNIEnv* e,jclass,jbyteArray state,jbyteArray program){
  try{const auto image=reverse_pc::rxe::decode(bytes(e,program));const auto source=reverse_pc::rxe::disassemble(image);auto runtime=reverse_os::restore(bytes(e,state));const auto lines=runtime.execute(source);return output(e,reverse_os::packet(lines,reverse_os::save(runtime)));}
  catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_appSeed(JNIEnv* e,jclass,jbyteArray program){
  try{return output(e,reverse_pc::rxe_app::decode(bytes(e,program)).seed);}
  catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_appTurn(JNIEnv* e,jclass,jbyteArray state,jbyteArray program,jbyteArray message,jint language){
  try{const auto text=bytes(e,message);const auto result=reverse_pc::rxe_app::turn(bytes(e,program),bytes(e,state),std::string(text.begin(),text.end()),unsigned(language));return output(e,reverse_os::packet(result.text,result.state));}
  catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_guestInitialize(JNIEnv* e,jclass,jbyteArray elf,jlong ram,jint overrides){
  try{
    reverse_pc::guest::MinimalGuest guest{std::uint64_t(ram),std::size_t(overrides)};auto image=bytes(e,elf);
    if(!elf)image=reverse_pc::guest::demo_elf(std::uint64_t(ram));
    guest.load_elf(std::move(image));return output(e,guest_packet(guest));
  }catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_guestInitializeSystem(JNIEnv* e,jclass,jbyteArray image,jint kind){
  try{
    reverse_pc::guest::MinimalGuest guest(128*1024*1024,65536);
    if(kind==0){reverse_pc::guest::require(!image,"Built-in system check does not accept an image");guest.load_elf(reverse_pc::guest::system_demo_elf(),true);}
    else if(kind==1)guest.load_elf(bytes(e,image),true);
    else if(kind==2)guest.load_linux_image(bytes(e,image));
    else if(kind==3){reverse_pc::guest::require(!image,"Built-in FP check does not accept an image");guest.load_elf(reverse_pc::guest::fp_demo_elf());}
    else if(kind==4){reverse_pc::guest::require(!image,"Built-in graphics check does not accept an image");guest.load_linux_image(reverse_pc::guest::pci_graphics_witness_image(),{},"console=ttyAMA0",nullptr,true);guest.enable_adaptive_memory();}
    else if(kind==5){reverse_pc::guest::require(!image,"Built-in counter check does not accept an image");guest=reverse_pc::guest::counter_witness();}
    else if(kind==6){reverse_pc::guest::require(!image,"Built-in counter cadence check does not accept an image");guest=reverse_pc::guest::counter_prescaled_witness();}
    else throw std::invalid_argument("Unsupported system image kind");
    return output(e,guest_packet(guest));
  }catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_reversepc_os_NativeRuntime_guestInspect(JNIEnv* e,jclass,jbyteArray state){
  try{const auto guest=reverse_pc::guest::MinimalGuest::from_snapshot(bytes(e,state));return e->NewStringUTF(guest.statistics().c_str());}
  catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_guestRun(JNIEnv* e,jclass,jbyteArray state,jint steps){
  try{
    reverse_pc::guest::require(steps>0&&steps<=10000,"Android guest step budget must be 1..10000");
    auto guest=reverse_pc::guest::MinimalGuest::from_snapshot(bytes(e,state));const auto run=guest.run(unsigned(steps));
    return output(e,guest_packet(guest,&run));
  }catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_guestXi(JNIEnv* e,jclass,jbyteArray state,jstring program){
  try{auto guest=reverse_pc::guest::MinimalGuest::from_snapshot(bytes(e,state));const auto lines=guest.apply_xi_delta(source(e,program));return output(e,guest_packet(guest,nullptr,lines));}
  catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_guestInput(JNIEnv* e,jclass,jbyteArray state,jstring text){
  try{auto guest=reverse_pc::guest::MinimalGuest::from_snapshot(bytes(e,state));guest.console_input(source(e,text));return output(e,guest_packet(guest));}
  catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_guestFileOperation(JNIEnv* e,jclass,jstring input,jstring candidate,
                                                      jint operation,jint steps,jstring text){
  try{
    reverse_pc::guest::require(operation>=0&&steps>=0,"Invalid guest file request");
    const auto report=reverse_pc::guest::guest_file_operation(source(e,input),
      candidate?source(e,candidate):std::string{},unsigned(operation),unsigned(steps),
      text?source(e,text):std::string{});
    return output(e,std::vector<std::uint8_t>(report.begin(),report.end()));
  }catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_guestFileDevices(JNIEnv* e,jclass,jstring input,jstring candidate,
                                                   jint operation,jint steps,jstring text,jstring disk_path){
  try{
    reverse_pc::guest::require(operation>=0&&operation<=6&&steps>=0,"Invalid device file request");
    const auto path=disk_path?source(e,disk_path):std::string{};
    const auto disk=guest_disk(path);
    std::string report;
    if(operation==6){reverse_pc::guest::require(bool(disk),"No disk selected");report="{\"ok\":true,\"disk_bytes\":"+std::to_string(disk->size())+",\"disk_sha256\":"+reverse_pc::guest::quote(disk->sha256())+"}";}
    else report=reverse_pc::guest::guest_file_operation(source(e,input),candidate?source(e,candidate):std::string{},unsigned(operation),unsigned(steps),text?source(e,text):std::string{},disk);
    return output(e,std::vector<std::uint8_t>(report.begin(),report.end()));
  }catch(const std::exception& x){throw_java(e,x);return nullptr;}
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_reversepc_os_NativeRuntime_guestSessionOpen(JNIEnv* e,jclass,jstring input,jstring disk_path){
  try{return jlong(guest_sessions().open(source(e,input),guest_disk(disk_path?source(e,disk_path):std::string{})));}
  catch(const std::exception& x){throw_java(e,x);return 0;}
}
extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_reversepc_os_NativeRuntime_guestSessionOperation(JNIEnv* e,jclass,jlong handle,jint operation,jint steps,jstring text,jstring candidate,jlong revision){
  try{
    reverse_pc::guest::require(handle>0&&operation>=0&&operation<=8&&steps>=0&&revision>=0,"Invalid resident guest request");
    const auto result=guest_sessions().operation(std::uint64_t(handle),unsigned(operation),std::uint64_t(steps),text?source(e,text):std::string{},candidate?source(e,candidate):std::string{},std::uint64_t(revision));
    return output(e,std::vector<std::uint8_t>(result.begin(),result.end()));
  }catch(const std::exception& x){throw_java(e,x);return nullptr;}
}
extern "C" JNIEXPORT void JNICALL
Java_com_reversepc_os_NativeRuntime_guestSessionClose(JNIEnv* e,jclass,jlong handle){
  try{reverse_pc::guest::require(handle>0,"Invalid resident guest handle");guest_sessions().close(std::uint64_t(handle));}
  catch(const std::exception& x){throw_java(e,x);}
}


