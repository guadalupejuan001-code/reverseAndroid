// Initialize an explicit new boot epoch with the parent's exact persistent drive.
// No RVG bytes are patched and no original engine source is changed.
#include "reverseandroid_power_cycle.hpp"
#include "guest_file_session.hpp"
#include <filesystem>
#include <iostream>
using namespace reverse_pc::guest;
static void write_text(const std::filesystem::path& path,const std::string& value){std::ofstream out(path,std::ios::binary);out<<value;out.close();require(bool(out),"Cannot publish power-cycle proof");}
int main(int argc,char** argv){try{
  require(argc==7,"Expected parent.rvg original-disk initrd output parent-sha initrd-sha");
  const std::string source_sha=argv[5],rd_sha=argv[6];
  require(source_sha=="7ecff7c2d460c582eeb3cbdf47fdd6686790cff2add75cd6c7437af7c9b9c3ec","Power cycle must derive from the fully admitted C75 final boundary");
  const auto root=std::filesystem::path(argv[4]);require(!std::filesystem::exists(root),"Output epoch already exists");
  std::cerr<<"POWER_CYCLE_PHASE verify_parent_and_original_disk\n";
  auto input=guest_read_file(argv[1],MinimalGuest::max_view_snapshot);
  require(GuestDiskSha256::hash(input)==source_sha,"Wrong parent checkpoint");
  const auto disk=GuestBlockImage::open(argv[2]);
  require(disk->size()==10888413184&&disk->sha256()=="4c2eaf3fa7d81e9649072fc81f0651f4129bd9f41d4a9f9964cc627741a7b92e","Original authenticated disk changed");
  auto parent=MinimalGuest::from_snapshot_view(input,disk,MinimalGuest::max_view_snapshot);
  std::size_t compared=0;parent.snapshot_to([&](const std::uint8_t* p,std::size_t n){require(compared+n<=input.size()&&std::equal(p,p+n,input.begin()+compared),"Parent roundtrip changed bytes");compared+=n;});require(compared==input.size(),"Parent roundtrip extent");
  require(parent.logical_ram()==2147483648&&parent.registers().instructions==88895950532&&!parent.registers().halted,"Wrong admitted parent machine");
  const auto clock=parent.machine().counter_latency;
  require(clock.enabled&&clock.instructions_per_tick==64&&GuestSha256::hash(Bytes(clock.seed.begin(),clock.seed.end()))=="699e17ffab1ef3618567ee928b25cdc0b6c7feed3d520147e9111c76a751a6cb","Original clock profile changed");
  const std::size_t at=360;require(std::string(input.begin()+at,input.begin()+at+4)=="RVI3","Wrong immutable media container");
  const auto kn=get(input,at+8,8),dt=get(input,at+16,8),rn=get(input,at+24,8),an=get(input,at+32,8);
  const Bytes kernel(input.begin()+at+48,input.begin()+at+48+kn);
  const Bytes original_rd(input.begin()+at+48+kn+dt,input.begin()+at+48+kn+dt+rn);
  const auto args_at=at+48+kn+dt+rn;const std::string args(input.begin()+args_at,input.begin()+args_at+an);
  require(GuestSha256::hash(kernel)=="f722bdd809bfd3485917a564b053b8a6d4938408734091f420c3297a8e3b20c8","Original kernel changed");
  require(GuestSha256::hash(original_rd)=="5278d736d20865d82be9783593690350496d1bcccb157769795316c5e527eefb","Original ramdisk changed");
  const auto rd=guest_read_file(argv[3],MinimalGuest::max_linux_initrd);require(GuestSha256::hash(rd)==rd_sha,"Wrong derived ramdisk");
  const auto bootconfig=[](const Bytes& bytes){require(bytes.size()>=20&&std::string(bytes.end()-12,bytes.end())=="#BOOTCONFIG\n","Missing bootconfig");const auto n=get(bytes,bytes.size()-20,4);require(n&&n<=32767&&n<=bytes.size()-20,"Bootconfig extent");const auto start=bytes.size()-20-n;std::uint32_t sum=0;for(std::size_t i=start;i<bytes.size()-20;++i)sum+=bytes[i];require(sum==get(bytes,bytes.size()-16,4),"Bootconfig checksum");auto end=bytes.size()-20;while(end>start&&!bytes[end-1])--end;return std::make_pair(start,std::string(bytes.begin()+start,bytes.begin()+end));};
  const auto old_config=bootconfig(original_rd),new_config=bootconfig(rd);
  require(old_config.first==new_config.first&&std::equal(original_rd.begin(),original_rd.begin()+old_config.first,rd.begin()),"Original CPIO payload changed");
  require(old_config.second.back()=='\n'&&new_config.second==old_config.second+"androidboot.vsock_lights_port=6900\n","Boot profile changed beyond the single lights port");
  Bytes().swap(input);
  MinimalGuest fresh(2147483648,0);fresh.load_linux_image(kernel,rd,args,disk,true);
  fresh.set_host_snapshot_transport_capacity(MinimalGuest::max_explicit_adaptive_snapshot);
  fresh.enable_counter_latency(clock.seed);fresh.configure_counter_rate(64);
  std::cerr<<"POWER_CYCLE_PHASE retain_completed_persistent_drive_writes_via_original_XI\n";
  const auto drive=retain_persistent_drive(parent,fresh);
  require(fresh.registers().instructions==0&&fresh.machine().el==1&&fresh.machine().ticks==0,"Fresh CPU or counter was advanced");
  std::cerr<<"POWER_CYCLE_PHASE verify_original_snapshot_roundtrip\n";
  auto saved=fresh.snapshot();auto restored=MinimalGuest::from_snapshot_view(saved,disk,MinimalGuest::max_explicit_adaptive_snapshot);
  compared=0;restored.snapshot_to([&](const std::uint8_t* p,std::size_t n){require(compared+n<=saved.size()&&std::equal(p,p+n,saved.begin()+compared),"Fresh original roundtrip changed bytes");compared+=n;});require(compared==saved.size(),"Fresh roundtrip extent");
  const auto snapshot_sha=GuestDiskSha256::hash(saved);Bytes().swap(saved);
  std::filesystem::create_directories(root);guest_write_snapshot_candidate(fresh,(root/"current.rvg").string());
  const std::string proof="{\"schema\":\"reverseandroid-hardware-power-cycle-v1\",\"parent_checkpoint_sha256\":"+quote(source_sha)+",\"parent_instructions\":88895950532,\"checkpoint_sha256\":"+quote(snapshot_sha)+",\"instructions\":0,\"parent_original_roundtrip_verified\":true,\"fresh_original_roundtrip_verified\":true,\"original_disk_sha256\":"+quote(disk->sha256())+",\"persistent_drive_delta_sha256\":"+quote(drive.sha256)+",\"persistent_drive_pages\":"+std::to_string(drive.pages)+",\"persistent_drive_xor_bytes\":"+std::to_string(drive.bytes)+",\"effective_persistent_drive_unchanged\":true,\"volatile_memory_reset\":true,\"prior_init_proof_reused\":false,\"live_android_properties_written\":false,\"logical_ram_bytes\":2147483648,\"counter_instructions_per_tick\":64,\"counter_seed_preserved\":true,\"derived_initrd_sha256\":"+quote(rd_sha)+",\"source_cpio_payload_unchanged\":true,\"bootargs_unchanged\":true,\"added_bootconfig\":{\"androidboot.vsock_lights_port\":\"6900\"},\"guest\":"+fresh.statistics()+"}\n";
  write_text(root/"power-cycle.json",proof);std::cout<<proof;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
