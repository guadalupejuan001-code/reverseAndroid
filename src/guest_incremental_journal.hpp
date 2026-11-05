#pragma once
// A bounded host journal of ORIGINAL canonical RVG components/MXIM records.
// Recovery reconstructs the original RVG, including its original checksum,
// before the unchanged MinimalGuest::from_snapshot decoder is called.
#include "guest_checkpoint_records.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>

namespace reverse_pc::guest {
class GuestJournalCompaction:public std::runtime_error{public:using std::runtime_error::runtime_error;};
class GuestIncrementalJournal {
 public:
  static constexpr unsigned max_entries=16;
  static constexpr std::size_t max_payload=64*1024*1024;
  using Hook=std::function<void(const std::string&)>;
  explicit GuestIncrementalJournal(std::string source,std::size_t capacity,Hook hook={})
      :source_(std::move(source)),root_(source_+".journal"),capacity_(capacity),hook_(std::move(hook)){}
  unsigned entries()const noexcept{return selected_.sequence;}
  std::size_t journal_bytes()const noexcept{return journal_bytes_;}
  unsigned pending_cleanup()const noexcept{return pending_cleanup_;}
  bool rolled_back()const noexcept{return rolled_back_;}
  bool initialized()const noexcept{return initialized_;}
  std::size_t logical_bytes()const{return validate(index_);}
  const std::string& base_sha()const noexcept{return base_sha_;}
  void sync_published_base_directory()const{
    sync_dir(root_.parent_path().empty()?std::filesystem::path("."):root_.parent_path());
  }

  // No journal means no extra base scan on the ordinary C44 load path.
  bool recover(){
    if(!std::filesystem::exists(root_))return false;
    require(stamp(source_).size<=capacity_,"Checkpoint base exceeds journal capacity");
    require(std::filesystem::is_directory(root_)&&!std::filesystem::is_symlink(root_),"Invalid checkpoint journal directory");
    std::string declared_base;bool identity_valid=false;
    try{std::istringstream mark(read_text(root_/"identity",128));std::string magic;
      identity_valid=bool(mark>>magic>>declared_base)&&magic=="RVJROOT1"&&sha_valid(declared_base)&&end(mark);
    }catch(const std::exception&){}
    base_sha_=hash_file(source_);
    std::vector<Head> candidates;bool current_head_bad=false;unsigned requested_sequence=0;
    for(const auto* name:{"head","previous"})try{auto head=read_head(root_/name);requested_sequence=std::max(requested_sequence,head.sequence);candidates.push_back(std::move(head));}
      catch(const std::exception&){if(std::string(name)=="head")current_head_bad=true;}
    if(identity_valid&&declared_base!=base_sha_&&candidates.empty()){rolled_back_=false;return false;} // A replacement RVG supersedes this history.
    require((identity_valid&&declared_base==base_sha_)||!candidates.empty(),"Journal identity and both recovery pointers are unverifiable");
    base_index_=parse_base();index_=base_index_;selected_={0,zeros()};
    limit_=std::min(max_payload,std::max<std::size_t>(1024*1024,std::filesystem::file_size(source_)/4));
    std::sort(candidates.begin(),candidates.end(),[](const Head& a,const Head& b){return a.sequence>b.sequence;});
    for(const auto& head:candidates){
      try{
        auto chain=read_chain(head);Index candidate=base_index_;Head good{0,zeros()};std::size_t bytes=0;
        for(const auto& meta:chain){
          try{
            const auto data=generation(meta.sequence)/"data";
            require(std::filesystem::file_size(data)==meta.bytes&&hash_file(data)==meta.payload,"Incomplete checkpoint journal payload");
            auto next=candidate;apply(data,meta.count,next);validate(next);candidate=std::move(next);
            good={meta.sequence,meta.sha};bytes+=meta.bytes;
          }catch(const std::exception&){rolled_back_=true;break;}
        }
        if(good.sequence>selected_.sequence){selected_=good;index_=std::move(candidate);journal_bytes_=bytes;}
        if(good.sequence<head.sequence)rolled_back_=true;
      }catch(const std::exception&){rolled_back_=true;}
    }
    rolled_back_=current_head_bad||selected_.sequence<requested_sequence;
    try{observed_head_=read_text(root_/"head",256);}catch(const std::exception&){observed_head_.clear();}
    initialized_=true;stamp_=stamp(source_);return selected_.sequence!=0;
  }
  Bytes recovered_bytes()const{
    const auto size=validate(index_);Bytes out;out.reserve(size);
    emit(index_,[&](const std::uint8_t* data,std::size_t n){out.insert(out.end(),data,data+n);});
    require(out.size()==size,"Recovered RVG length differs");return out;
  }
  std::size_t commit(const MinimalGuest& guest,const MinimalGuest& previous){
    initialize(previous);
    require(stamp(source_)==stamp_,"Checkpoint base changed during the resident session");
    const auto actual_head=std::filesystem::exists(root_/"head")?read_text(root_/"head",256):std::string{};
    auto selected_text=head_text(selected_);
    require(actual_head==observed_head_||actual_head==selected_text||(!attempted_head_.empty()&&actual_head==attempted_head_),"Checkpoint journal advanced outside this resident session");
    if(selected_.sequence>=max_entries)throw GuestJournalCompaction("Checkpoint journal entry limit");
    std::vector<Change> changes;std::size_t estimated=10;
    const auto logical=GuestCheckpointRecords::patch(guest,previous,[&](const std::string& key,const std::string& data,bool removed){
      require(key_valid(key)&&key!="I","Invalid checkpoint journal record key");
      const auto count=key.size()+data.size()+32;
      if(count>limit_-std::min(limit_,estimated))throw GuestJournalCompaction("Checkpoint journal payload limit");
      estimated+=count;changes.push_back({key,data,removed});
    });
    if(estimated>limit_-std::min(limit_,journal_bytes_))throw GuestJournalCompaction("Checkpoint journal storage limit");
    // Re-anchor a recovered prefix before replacing any unusable tail files.
    if(rolled_back_||actual_head!=selected_text){publish_pointer("previous",selected_);publish_pointer("head",selected_);observed_head_=std::move(selected_text);}
    cleanup_orphans();
    const auto number=selected_.sequence+1;const auto pending=temporary_directory();
    try{
      Writer data(pending/"data");data.text("RVJD1\n");
      for(const auto& change:changes){
        if(change.removed)data.text("D "+change.key+"\n");
        else{data.text("S "+change.key+" "+std::to_string(change.data.size())+"\n");data.text(change.data);data.text("\n");}
      }
      data.text("END\n");const auto payload_sha=data.finish();
      checkpoint("payload_synced");
      const auto meta_text="RVJM1 "+base_sha_+" "+std::to_string(number)+" "+selected_.sha+" "+payload_sha+" "+std::to_string(data.bytes())+" "+std::to_string(changes.size())+"\n";
      Writer metadata(pending/"meta");metadata.text(meta_text);const auto meta_sha=metadata.finish();sync_dir(pending);
      auto next=index_;apply(pending/"data",changes.size(),next);require(validate(next)==logical,"Journal recipe differs from the original snapshot size");
      const auto target=generation(number);std::error_code ignored;std::filesystem::remove_all(target,ignored);
      require(!std::filesystem::exists(target),"Cannot replace an unselected journal tail");
      std::filesystem::rename(pending,target);sync_dir(root_);checkpoint("generation_synced");
      // Extents parsed above must point to the immutable published location.
      for(auto& item:next)if(item.second.file==pending/"data")item.second.file=target/"data";
      Head current{number,meta_sha};attempted_head_=head_text(current);
      publish_pointer("previous",selected_);checkpoint("before_head");publish_pointer("head",current);
      // Everything needed for recovery is durable before the in-memory ack.
      selected_=std::move(current);observed_head_=std::move(attempted_head_);index_=std::move(next);journal_bytes_+=data.bytes();rolled_back_=false;cleanup_orphans();
      return logical;
    }catch(...){std::error_code ignored;std::filesystem::remove_all(pending,ignored);throw;}
  }
  void retire(){
    // The canonical RVG has already been published and verified by the caller.
    // Detaching history is required; removal of its retired files is best effort.
    checkpoint("before_retire");archive_root();
    initialized_=false;selected_={0,zeros()};index_.clear();base_index_.clear();journal_bytes_=0;rolled_back_=false;observed_head_.clear();attempted_head_.clear();pending_cleanup_=0;
  }
  template<class Sink>void materialize(Sink&& sink)const{emit(index_,std::forward<Sink>(sink));}
 private:
  struct Head{unsigned sequence=0;std::string sha;};
  struct Meta{unsigned sequence=0;std::string parent,payload,sha;std::size_t bytes=0,count=0;};
  struct Extent{std::filesystem::path file;std::size_t offset=0,length=0;};
  struct Change{std::string key,data;bool removed;};
  struct Less{
    static unsigned rank(const std::string& key){switch(key[0]){case 'H':return 0;case 'I':return 1;case 'C':return 2;case 'B':case 'M':return 3;case 'P':return 4;case 'O':return 5;case 'Q':return 6;case 'S':return 7;case 'F':return 8;default:return 9;}}
    bool operator()(const std::string& a,const std::string& b)const{
      const auto x=rank(a),y=rank(b);if(x!=y)return x<y;
      if(a[0]!=b[0])return a[0]<b[0];if(a.size()!=b.size())return a.size()<b.size();return a<b;
    }
  };
  using Index=std::map<std::string,Extent,Less>;
  struct Stamp{std::uint64_t device,inode,size;std::int64_t modified,changed,modified_ns,changed_ns;
    bool operator==(const Stamp& b)const{return device==b.device&&inode==b.inode&&size==b.size&&modified==b.modified&&changed==b.changed&&modified_ns==b.modified_ns&&changed_ns==b.changed_ns;}};
  static Stamp stamp(const std::filesystem::path& path){
    struct stat s{};require(::stat(path.c_str(),&s)==0&&S_ISREG(s.st_mode),"Checkpoint base is not a regular file");
#if defined(__APPLE__)
    return {std::uint64_t(s.st_dev),std::uint64_t(s.st_ino),std::uint64_t(s.st_size),s.st_mtimespec.tv_sec,s.st_ctimespec.tv_sec,s.st_mtimespec.tv_nsec,s.st_ctimespec.tv_nsec};
#else
    return {std::uint64_t(s.st_dev),std::uint64_t(s.st_ino),std::uint64_t(s.st_size),s.st_mtim.tv_sec,s.st_ctim.tv_sec,s.st_mtim.tv_nsec,s.st_ctim.tv_nsec};
#endif
  }
  class Writer{
   public:
    explicit Writer(const std::filesystem::path& path){fd_=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);require(fd_>=0,"Cannot stage checkpoint journal component");}
    ~Writer(){if(fd_>=0)::close(fd_);}
    void text(const std::string& value){write(reinterpret_cast<const std::uint8_t*>(value.data()),value.size());}
    void write(const std::uint8_t* data,std::size_t length){
      std::size_t at=0;while(at<length){const auto n=::write(fd_,data+at,length-at);if(n<0&&errno==EINTR)continue;require(n>0,"Incomplete checkpoint journal write");at+=std::size_t(n);}
      sha_.update(data,length);bytes_+=length;
    }
    std::string finish(){const int synced=::fsync(fd_),closed=::close(fd_);fd_=-1;require(synced==0&&closed==0,"Cannot sync checkpoint journal component");return sha_.finish();}
    std::size_t bytes()const{return bytes_;}
   private:int fd_=-1;GuestSha256 sha_;std::size_t bytes_=0;
  };
  static std::string zeros(){return std::string(64,'0');}
  static bool sha_valid(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
  static bool end(std::istringstream& in){std::string extra;return !(in>>extra);}
  static std::uint64_t decimal(const std::string& s,std::uint64_t maximum){
    require(!s.empty()&&(s.size()==1||s[0]!='0')&&s.find_first_not_of("0123456789")==std::string::npos,"Invalid journal decimal field");
    std::uint64_t result=0;for(char c:s){require(result<=maximum/10&&(result<maximum/10||unsigned(c-'0')<=maximum%10),"Journal decimal field exceeds its bound");result=result*10+unsigned(c-'0');}return result;
  }
  static bool key_valid(const std::string& key){
    if(key.empty()||key.size()>8193)return false;
    if(key.size()==1)return std::string("HICBMFS").find(key[0])!=std::string::npos;
    if(std::string("POQ").find(key[0])==std::string::npos||key[1]=='0'&&key.size()>2||key.find_first_not_of("0123456789",1)!=std::string::npos)return false;
    if(key[0]=='P')try{decimal(key.substr(1),UINT64_MAX>>12);}catch(const std::exception&){return false;}return true;
  }
  static void sync_dir(const std::filesystem::path& path){
    const int fd=::open(path.c_str(),O_RDONLY|O_DIRECTORY);require(fd>=0,"Cannot open checkpoint journal directory");const int result=::fsync(fd),closed=::close(fd);require(result==0&&closed==0,"Cannot sync checkpoint journal directory");
  }
  static std::string read_text(const std::filesystem::path& file,std::size_t maximum){
    std::ifstream in(file,std::ios::binary|std::ios::ate);require(bool(in),"Missing checkpoint journal metadata");const auto size=in.tellg();
    require(size>=0&&std::uint64_t(size)<=maximum,"Oversized checkpoint journal metadata");std::string out(std::size_t(size),'\0');in.seekg(0);in.read(out.data(),std::streamsize(out.size()));require(bool(in),"Truncated checkpoint journal metadata");return out;
  }
  static std::string hash_file(const std::filesystem::path& file){
    std::ifstream in(file,std::ios::binary);require(bool(in),"Cannot hash checkpoint journal input");std::array<std::uint8_t,65536> b{};GuestSha256 sha;
    while(in){in.read(reinterpret_cast<char*>(b.data()),b.size());sha.update(b.data(),std::size_t(in.gcount()));}require(in.eof(),"Checkpoint journal hash read failed");return sha.finish();
  }
  static std::string hash_text(const std::string& value){GuestSha256 sha;sha.update(reinterpret_cast<const std::uint8_t*>(value.data()),value.size());return sha.finish();}
  static std::string line(std::ifstream& input,std::size_t maximum=20000){
    std::vector<char> buffer(maximum+2);input.getline(buffer.data(),std::streamsize(buffer.size()));
    require(bool(input)&&input.gcount()>0&&std::size_t(input.gcount())<=maximum+1,"Invalid checkpoint journal record line");
    std::string out(buffer.data(),std::size_t(input.gcount())-1);require(out.find('\0')==std::string::npos,"NUL in checkpoint record descriptor");return out;
  }
  static std::string extent_text(const Extent& e,std::size_t maximum){
    require(e.length<=maximum,"Oversized checkpoint component");std::ifstream in(e.file,std::ios::binary);require(bool(in),"Missing checkpoint component");in.seekg(std::streamoff(e.offset));std::string out(e.length,'\0');in.read(out.data(),std::streamsize(out.size()));require(bool(in),"Truncated checkpoint component");return out;
  }
  Index parse_base()const{
    const auto file_size=std::filesystem::file_size(source_);require(file_size<=capacity_&&file_size>=352,"Checkpoint base exceeds journal capacity");
    std::ifstream input(source_,std::ios::binary);Bytes header(8);input.read(reinterpret_cast<char*>(header.data()),8);
    require(bool(input)&&header[0]=='R'&&header[1]=='V'&&header[2]=='G'&&header[3]>='1'&&header[3]<='3',"Invalid journal RVG base");
    const unsigned version=header[3]-'0';const auto hsize=8+8*(41+version);header.resize(hsize);input.read(reinterpret_cast<char*>(header.data()+8),hsize-8);require(bool(input),"Truncated journal RVG header");
    Index out;out["H"]={source_,0,hsize};std::size_t at=hsize;
    auto add=[&](const std::string& key,std::uint64_t length){require(at<=file_size-8&&length<=file_size-8-at,"Invalid journal RVG component extent");out[key]={source_,at,std::size_t(length)};at+=std::size_t(length);};
    add("I",get(header,320,8));add("C",get(header,328,8));const auto memory_at=at;add("B",get(header,336,8));const auto memory_end=at;
    add("S",version>=2?get(header,344,8):0);add("F",version==3?get(header,352,8):0);require(at+8==file_size,"Invalid journal RVG total extent");
    if(get(header,16,8)==0){
      input.seekg(std::streamoff(memory_at));const auto prefix=line(input),second=line(input);
      require(prefix.compare(0,6,"MXIM2 ")==0&&second.compare(0,6,"MXIM3 ")==0,"Adaptive checkpoint base is not canonical MXIM2/MXIM3");
      const auto records_at=std::size_t(input.tellg());require(records_at<=memory_end,"Invalid compact memory prefix extent");out.erase("B");out["M"]={source_,memory_at,records_at-memory_at};
      std::string last;
      while(std::size_t(input.tellg())<memory_end){
        const auto offset=std::size_t(input.tellg());const auto record=line(input);const auto finish=std::size_t(input.tellg());
        const auto space=record.find(' ',2);require(record.size()>=4&&record[1]==' '&&space!=std::string::npos&&finish<=memory_end,"Malformed compact memory record");
        const auto key=record.substr(0,1)+record.substr(2,space-2);
        require(key_valid(key)&&std::string("POQ").find(key[0])!=std::string::npos&&(last.empty()||Less{}(last,key)),"Noncanonical compact memory record order");
        require(out.emplace(key,Extent{source_,offset,finish-offset}).second,"Duplicate compact memory record");last=key;
      }
      require(std::size_t(input.tellg())==memory_end,"Compact memory record exceeds its RVG extent");
    }
    validate(out);return out;
  }
  std::size_t validate(const Index& index)const{
    for(const auto* key:{"H","I","C","S","F"})require(index.count(key),"Missing journal RVG component");
    const auto text=extent_text(index.at("H"),360);Bytes h(text.begin(),text.end());
    require(h.size()>=344&&h[0]=='R'&&h[1]=='V'&&h[2]=='G'&&h[3]>='1'&&h[3]<='3',"Invalid reconstructed RVG header");
    const unsigned version=h[3]-'0';require(h.size()==8+8*(41+version),"Reconstructed RVG header extent differs");
    require(index.at("I").length==get(h,320,8)&&index.at("C").length==get(h,328,8)&&index.at("S").length==(version>=2?get(h,344,8):0)&&index.at("F").length==(version==3?get(h,352,8):0),"Reconstructed RVG component extent differs");
    require(index.count("M")+index.count("B")==1,"Ambiguous reconstructed memory representation");
    std::size_t memory=0,total=8;
    for(const auto& item:index){
      require(key_valid(item.first)&&item.second.length<=capacity_-std::min(capacity_,total),"Journal RVG exceeds capacity");total+=item.second.length;
      if(std::string("BMPOQ").find(item.first[0])!=std::string::npos)memory+=item.second.length;
      if(index.count("B"))require(std::string("POQM").find(item.first[0])==std::string::npos,"Legacy memory mixed with adaptive records");
    }
    require(total<=capacity_&&memory==get(h,336,8),"Reconstructed compact memory length differs");return total;
  }
  static void apply(const std::filesystem::path& file,std::size_t count,Index& index){
    require(count<=1000000,"Too many checkpoint journal records");std::ifstream input(file,std::ios::binary);require(bool(input)&&line(input,32)=="RVJD1","Invalid checkpoint journal payload");
    const auto maximum=std::filesystem::file_size(file);std::map<std::string,bool> touched;
    for(std::size_t i=0;i<count;++i){
      const auto descriptor=line(input,8300);std::istringstream fields(descriptor);std::string kind,key,length;
      require(bool(fields>>kind>>key)&&key_valid(key)&&key!="I"&&touched.emplace(key,true).second,"Invalid or duplicate checkpoint patch key");
      if(kind=="D"){require(end(fields)&&key.size()>1&&index.erase(key)==1,"Invalid checkpoint record deletion");}
      else{
        require(kind=="S"&&bool(fields>>length)&&end(fields),"Invalid checkpoint replacement descriptor");
        const auto size=std::size_t(decimal(length,max_payload)),offset=std::size_t(input.tellg());
        require(offset<maximum&&size<maximum-offset,"Truncated checkpoint replacement");input.seekg(std::streamoff(size),std::ios::cur);char newline=0;input.get(newline);require(bool(input)&&newline=='\n',"Incomplete checkpoint replacement terminator");index[key]={file,offset,size};
      }
    }
    require(line(input,32)=="END"&&input.peek()==std::char_traits<char>::eof(),"Checkpoint payload has an incomplete or extra tail");
  }
  template<class Sink>void emit(const Index& index,Sink&& sink)const{
    const auto expected=validate(index);std::uint64_t checksum=UINT64_C(14695981039346656037);std::size_t emitted=0;std::array<std::uint8_t,65536> buffer{};
    std::filesystem::path current;std::ifstream input;
    for(const auto& item:index){const auto& e=item.second;if(!e.length)continue;
      if(current!=e.file){input.close();input.clear();input.open(e.file,std::ios::binary);require(bool(input),"Cannot read journal checkpoint component");current=e.file;}
      input.clear();input.seekg(std::streamoff(e.offset));std::size_t remaining=e.length;
      while(remaining){const auto n=std::min(remaining,buffer.size());input.read(reinterpret_cast<char*>(buffer.data()),std::streamsize(n));require(bool(input),"Incomplete recovered checkpoint component");
        for(std::size_t i=0;i<n;++i){checksum^=buffer[i];checksum*=UINT64_C(1099511628211);}sink(buffer.data(),n);emitted+=n;remaining-=n;}
    }
    require(emitted+8==expected,"Recovered RVG stream length differs");Bytes tail;put(tail,checksum);sink(tail.data(),tail.size());
  }
  std::filesystem::path generation(unsigned sequence)const{return root_/("generation-"+std::to_string(sequence));}
  Head read_head(const std::filesystem::path& file)const{
    const auto text=read_text(file,256);const auto at=text.find('\n');require(at!=std::string::npos&&text.size()==at+66,"Invalid checkpoint journal head extent");
    const auto body=text.substr(0,at+1);require(hash_text(body)==text.substr(at+1,64)&&text.back()=='\n',"Checkpoint journal head checksum mismatch");
    std::istringstream fields(body);std::string magic,base,number,sha;
    require(bool(fields>>magic>>base>>number>>sha)&&end(fields)&&magic=="RVJH1"&&base==base_sha_&&sha_valid(sha),"Invalid checkpoint journal head");
    const auto sequence=unsigned(decimal(number,max_entries));require(sequence||sha==zeros(),"Invalid base-only journal head");return {sequence,sha};
  }
  std::vector<Meta> read_chain(const Head& head)const{
    std::vector<Meta> chain;std::string expected=head.sha;std::size_t total=0;
    for(unsigned i=head.sequence;i;--i){
      const auto text=read_text(generation(i)/"meta",512);require(hash_text(text)==expected,"Checkpoint generation metadata checksum mismatch");
      std::istringstream fields(text);std::string magic,base,number,parent,payload,bytes,count;
      require(bool(fields>>magic>>base>>number>>parent>>payload>>bytes>>count)&&end(fields)&&magic=="RVJM1"&&base==base_sha_&&decimal(number,max_entries)==i&&sha_valid(parent)&&sha_valid(payload),"Invalid checkpoint generation metadata");
      const auto size=std::size_t(decimal(bytes,max_payload));require(size<=limit_-std::min(limit_,total),"Checkpoint chain exceeds its storage bound");total+=size;
      chain.push_back({i,parent,payload,expected,size,std::size_t(decimal(count,1000000))});expected=parent;
    }
    require(expected==zeros(),"Checkpoint chain is not bound to its base");std::reverse(chain.begin(),chain.end());return chain;
  }
  std::filesystem::path temporary_directory(){
    auto pattern=(root_/".pending-XXXXXX").string();std::vector<char> value(pattern.begin(),pattern.end());value.push_back(0);require(::mkdtemp(value.data())!=nullptr,"Cannot stage checkpoint journal generation");return std::filesystem::path(value.data());
  }
  void publish_pointer(const std::string& name,const Head& head){
    auto pattern=(root_/".pointer-XXXXXX").string();std::vector<char> value(pattern.begin(),pattern.end());value.push_back(0);const int fd=::mkstemp(value.data());require(fd>=0,"Cannot stage checkpoint journal head");::close(fd);::unlink(value.data());
    try{Writer file(value.data());file.text(head_text(head));file.finish();require(std::rename(value.data(),(root_/name).c_str())==0,"Cannot publish checkpoint journal head");sync_dir(root_);}
    catch(...){::unlink(value.data());throw;}
  }
  void initialize(const MinimalGuest& previous){
    if(initialized_)return;
    require(stamp(source_).size<=capacity_,"Checkpoint base exceeds journal capacity");
    const auto canonical=previous.snapshot_sha256();const auto disk=hash_file(source_);
    if(canonical!=disk)throw GuestJournalCompaction("Checkpoint base must first be normalized to the original canonical RVG");
    base_sha_=disk;base_index_=parse_base();index_=base_index_;limit_=std::min(max_payload,std::max<std::size_t>(1024*1024,std::filesystem::file_size(source_)/4));
    // The immutable base must itself be durable before a journal can refer to it.
    const int base_fd=::open(source_.c_str(),O_RDONLY);require(base_fd>=0,"Cannot sync the checkpoint journal base");
    const int base_synced=::fsync(base_fd),base_closed=::close(base_fd);require(base_synced==0&&base_closed==0,"Cannot persist the checkpoint journal base");
    if(std::filesystem::exists(root_)){
      const auto identity=read_text(root_/"identity",128);
      require(!std::filesystem::is_symlink(root_)&&identity.compare(0,9,"RVJROOT1 ")==0,"Unrecognized checkpoint journal directory");
      if(identity=="RVJROOT1 "+base_sha_+"\n")require(read_head(root_/"head").sequence==0&&read_head(root_/"previous").sequence==0,"Recover the existing committed journal before writing");
      else{bool selected=false;for(const auto* name:{"head","previous"})try{selected=selected||read_head(root_/name).sequence!=0;}catch(const std::exception&){}
        require(!selected,"Recover the existing committed journal before writing");}
      archive_root();
    }
    const auto destination=root_;auto pattern=root_.string()+".initial-XXXXXX";std::vector<char> temporary(pattern.begin(),pattern.end());temporary.push_back(0);
    require(::mkdtemp(temporary.data())!=nullptr,"Cannot create checkpoint journal directory");root_=temporary.data();
    try{
      Writer identity(root_/"identity");identity.text("RVJROOT1 "+base_sha_+"\n");identity.finish();sync_dir(root_);
      selected_={0,zeros()};publish_pointer("previous",selected_);publish_pointer("head",selected_);
      std::filesystem::rename(root_,destination);root_=destination;sync_dir(root_.parent_path().empty()?std::filesystem::path("."):root_.parent_path());stamp_=stamp(source_);observed_head_=head_text(selected_);initialized_=true;
    }catch(...){root_=destination;std::error_code ignored;std::filesystem::remove_all(temporary.data(),ignored);throw;}
  }
  void archive_root(){
    if(!std::filesystem::exists(root_))return;
    auto pattern=root_.string()+".retired-XXXXXX";std::vector<char> temporary(pattern.begin(),pattern.end());temporary.push_back(0);
    require(::mkdtemp(temporary.data())!=nullptr,"Cannot retire checkpoint journal directory");
    try{sync_published_base_directory();std::filesystem::rename(root_,temporary.data());sync_published_base_directory();}
    catch(...){std::error_code ignored;std::filesystem::remove(temporary.data(),ignored);throw;}
    std::error_code ignored;std::filesystem::remove_all(temporary.data(),ignored);
  }
  void checkpoint(const std::string& phase){if(hook_)hook_(phase);}
  std::string head_text(const Head& head)const{
    const auto body="RVJH1 "+base_sha_+" "+std::to_string(head.sequence)+" "+head.sha+"\n";return body+hash_text(body)+"\n";
  }
  void cleanup_orphans()noexcept{
    pending_cleanup_=0;
    try{for(const auto& entry:std::filesystem::directory_iterator(root_)){
      const auto name=entry.path().filename().string();bool remove=name.compare(0,9,".pending-")==0||name.compare(0,9,".pointer-")==0;
      if(name.compare(0,11,"generation-")==0)try{remove=decimal(name.substr(11),max_entries)>selected_.sequence;}catch(const std::exception&){}
      if(remove){std::error_code error;std::filesystem::remove_all(entry.path(),error);if(error)++pending_cleanup_;}
    }}catch(const std::exception&){++pending_cleanup_;}
  }
  std::string source_;std::filesystem::path root_;std::size_t capacity_,limit_=max_payload,journal_bytes_=0;
  std::string base_sha_,observed_head_,attempted_head_;Head selected_{0,zeros()};Index base_index_,index_;Stamp stamp_{};bool initialized_=false,rolled_back_=false;unsigned pending_cleanup_=0;Hook hook_;
};
} // namespace reverse_pc::guest
