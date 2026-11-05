#define main c13_runner_main
#include "../tools/guest_c13_boot.cpp"
#undef main
int main(int argc,char** argv){try{
 require(argc==2,"Expected original C12 evidence");const auto state=guest_read_file(argv[1],MinimalGuest::max_adaptive_snapshot);const auto media=immutable_media(state);
 const auto original=guest_android_cuttlefish_normal_no_scx_bootargs()+" androidboot.boot_devices=30000000.pcie";
 const auto args=original+" "+avb_args;require(args.size()<=1024,"AVB profile too long");
 MinimalGuest fixture(128*1024*1024,0);fixture.load_linux_image(media.first,media.second,args,{},true);fixture.enable_adaptive_memory();
 std::array<std::uint8_t,32> seed{};seed[0]=1;fixture.enable_counter_latency(seed);fixture.configure_counter_rate(64);
 require(fixture.registers().instructions==0&&fixture.machine().el==1,"Not a fresh fixture");
 const auto saved=fixture.snapshot();auto restored=MinimalGuest::from_snapshot(saved);require(restored.snapshot()==saved,"Fresh AVB fixture not round-trip stable");
 const std::string bytes(saved.begin(),saved.end());require(bytes.find(args)!=std::string::npos,"AVB arguments absent from immutable image");
 require(guest_android_cuttlefish_normal_no_scx_bootargs().find("androidboot.vbmeta")==std::string::npos,"Historical profile changed");
 auto changed=state;changed.back()^=1;bool rejected=false;try{immutable_media(changed);}catch(const std::exception&){rejected=true;}require(rejected,"Altered C12 evidence accepted");
 require(GuestSha256::hash(state)==c12_sha,"Original evidence mutated");
 std::cout<<"{\"original_media_verified\":true,\"fresh_setup_roundtrip\":true,\"input_mutation_rejected\":true,\"historical_profile_unchanged\":true,\"bootargs_bytes\":"<<args.size()<<",\"guest_instructions\":0,\"real_disk_attached\":false,\"android_boot\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
