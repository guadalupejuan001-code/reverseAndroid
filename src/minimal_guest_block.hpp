#pragma once
// Immutable, content-addressed disk input. Guest writes live in XI/Ether.
// File blocks are verified against the attach-time hash on every cache miss.
#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef REVERSE_PC_OPENSSL_DISK_SHA
#include <openssl/evp.h>
#endif
namespace reverse_pc::guest {
class GuestSha256 {
  std::array<std::uint32_t,8> h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  std::array<std::uint8_t,64> b{};std::uint64_t total=0;unsigned used=0;
  static std::uint32_t r(std::uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
  void block(){
    static constexpr std::uint32_t k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::uint32_t w[64];for(unsigned i=0;i<16;i++)w[i]=(std::uint32_t(b[4*i])<<24)|(std::uint32_t(b[4*i+1])<<16)|(std::uint32_t(b[4*i+2])<<8)|b[4*i+3];
    for(unsigned i=16;i<64;i++)w[i]=w[i-16]+(r(w[i-15],7)^r(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(r(w[i-2],17)^r(w[i-2],19)^(w[i-2]>>10));
    auto a=h[0],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7],bb=h[1];
    for(unsigned i=0;i<64;i++){const auto t=v+(r(e,6)^r(e,11)^r(e,25))+((e&f)^(~e&g))+k[i]+w[i];const auto s=(r(a,2)^r(a,13)^r(a,22))+((a&bb)^(a&c)^(bb&c));v=g;g=f;f=e;e=d+t;d=c;c=bb;bb=a;a=t+s;}
    h[0]+=a;h[1]+=bb;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
  }
 public:
  void update(const std::uint8_t* p,std::size_t n){total+=n;while(n){const auto k=std::min<std::size_t>(n,64-used);std::copy(p,p+k,b.begin()+used);used+=unsigned(k);p+=k;n-=k;if(used==64){block();used=0;}}}
  std::string finish(){const auto bits=total*8;const std::uint8_t one=128,zero=0;update(&one,1);while(used!=56)update(&zero,1);std::uint8_t length[8];for(unsigned i=0;i<8;i++)length[i]=std::uint8_t(bits>>(56-8*i));update(length,8);std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto x:h)out<<std::setw(8)<<x;return out.str();}
  static std::string hash(const std::vector<std::uint8_t>& b){GuestSha256 h;h.update(b.data(),b.size());return h.finish();}
};
// Host disk authentication only. The original portable hash remains unchanged.
#ifdef REVERSE_PC_OPENSSL_DISK_SHA
class GuestDiskSha256 {
  std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> context_{EVP_MD_CTX_new(),EVP_MD_CTX_free};
 public:
  GuestDiskSha256(){
    if(!context_||EVP_DigestInit_ex(context_.get(),EVP_sha256(),nullptr)!=1)
      throw std::runtime_error("Cannot initialize host disk SHA-256");
  }
  void update(const std::uint8_t* data,std::size_t size){
    if(EVP_DigestUpdate(context_.get(),data,size)!=1)
      throw std::runtime_error("Cannot authenticate host disk bytes");
  }
  std::string finish(){
    std::array<unsigned char,EVP_MAX_MD_SIZE> bytes{};unsigned size=0;
    if(EVP_DigestFinal_ex(context_.get(),bytes.data(),&size)!=1||size!=32)
      throw std::runtime_error("Cannot finish host disk SHA-256");
    std::ostringstream out;out<<std::hex<<std::setfill('0');
    for(unsigned n=0;n<size;++n)out<<std::setw(2)<<unsigned(bytes[n]);return out.str();
  }
  static std::string hash(const std::vector<std::uint8_t>& bytes){
    GuestDiskSha256 hash;hash.update(bytes.data(),bytes.size());return hash.finish();
  }
};
#else
using GuestDiskSha256=GuestSha256;
#endif
class GuestBlockImage {
  static constexpr std::size_t block_size=65536;
  std::string path_,sha_;std::uint64_t size_=0;
  std::vector<std::string> hashes_;
  mutable std::ifstream file_;mutable std::mutex mutex_;
  mutable std::vector<std::uint8_t> cache_;mutable std::uint64_t cached_=UINT64_MAX;
  explicit GuestBlockImage(const std::string& path):path_(path),file_(path,std::ios::binary){
    if(!file_)throw std::runtime_error("Cannot open explicitly attached disk");
    file_.seekg(0,std::ios::end);const auto end=file_.tellg();
    if(end<=0||std::uint64_t(end)>(UINT64_C(1)<<40)||std::uint64_t(end)%512)throw std::runtime_error("Disk must be 512-byte aligned and at most 1 TiB");
    size_=std::uint64_t(end);file_.seekg(0);GuestDiskSha256 whole;
    for(std::uint64_t at=0;at<size_;at+=block_size){std::vector<std::uint8_t> b(std::size_t(std::min<std::uint64_t>(block_size,size_-at)));file_.read(reinterpret_cast<char*>(b.data()),std::streamsize(b.size()));if(!file_)throw std::runtime_error("Disk changed while attaching");whole.update(b.data(),b.size());hashes_.push_back(GuestDiskSha256::hash(b));}
    sha_=whole.finish();
  }
 public:
  static std::shared_ptr<const GuestBlockImage> open(const std::string& path){return std::shared_ptr<const GuestBlockImage>(new GuestBlockImage(path));}
  std::uint64_t size() const{return size_;}
  const std::string& sha256() const{return sha_;}
  std::vector<std::uint8_t> read(std::uint64_t offset,std::size_t length) const {
    if(offset>size_||length>size_-offset||length>1024*1024)throw std::runtime_error("Disk read outside bounded image");
    std::lock_guard<std::mutex> guard(mutex_);std::vector<std::uint8_t> out(length);std::size_t done=0;
    while(done<length){const auto at=offset+done,index=at/block_size;
      if(cached_!=index){std::vector<std::uint8_t> b(std::size_t(std::min<std::uint64_t>(block_size,size_-index*block_size)));file_.clear();file_.seekg(std::streamoff(index*block_size));file_.read(reinterpret_cast<char*>(b.data()),std::streamsize(b.size()));if(!file_||GuestDiskSha256::hash(b)!=hashes_.at(std::size_t(index)))throw std::runtime_error("Attached disk block changed; guest transaction rejected");cache_=std::move(b);cached_=index;}
      const auto in=std::size_t(at%block_size),n=std::min(length-done,cache_.size()-in);std::copy(cache_.begin()+in,cache_.begin()+in+n,out.begin()+done);done+=n;
    }return out;
  }
};
} // namespace reverse_pc::guest
