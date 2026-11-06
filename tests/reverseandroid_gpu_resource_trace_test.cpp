// Reuse the actual PCI client and AArch64 MMIO instructions from the original
// device tests. These are protocol fixtures, never Android boot evidence.
#define main original_pci_test_main
#include "minimal_guest_pci_test.cpp"
#undef main

int main(int argc, char** argv) { try {
  require(argc == 2, "Expected new fixture snapshot output");
  Bytes kernel(4096);field(kernel,0,0xd503201f,4);field(kernel,8,0x80000,8);
  field(kernel,16,kernel.size(),8);field(kernel,56,0x644d5241,4);
  MinimalGuest guest(128*1024*1024,0);
  guest.load_linux_image(kernel,{},"",nullptr,true);
  Client gpu(guest,1);
  auto create=command(0x101,40);field(create,24,42,4);field(create,28,2,4);
  field(create,32,1,4);field(create,36,1,4);
  check(gpu.gpu(create)==0x1100,"Create actual resource42");
  const auto pixels=UINT64_C(0x42000000);guest.write(pixels,0x00112233,4);
  auto attach=command(0x106,48);field(attach,24,42,4);field(attach,28,1,4);
  field(attach,32,pixels,8);field(attach,40,4,4);
  check(gpu.gpu(attach)==0x1100,"Attach actual resource42");
  auto transfer=command(0x105,56);setrect(transfer,1,1);field(transfer,48,42,4);
  auto scan=command(0x103,48);setrect(scan,1,1);field(scan,44,42,4);
  auto flush=command(0x104,48);setrect(flush,1,1);field(flush,40,42,4);
  check(gpu.gpu(transfer)==0x1100&&gpu.gpu(scan)==0x1100&&gpu.gpu(flush)==0x1100,"Actual frame commands");
  check(guest.display_rgb()==Bytes({0x11,0x22,0x33}),"Actual XI frame pixels");
  const auto original=guest.snapshot();
  guest=MinimalGuest::from_snapshot_view(original,nullptr,MinimalGuest::max_view_snapshot);
  check(guest.snapshot()==original&&guest.display_rgb()==Bytes({0x11,0x22,0x33}),"Original RVG lifecycle restore");
  auto invalidTransfer=transfer;field(invalidTransfer,48,99,4);
  auto invalidScan=scan;field(invalidScan,44,99,4);
  auto invalidFlush=flush;field(invalidFlush,40,99,4);
  check(gpu.gpu(invalidTransfer)==0x1203,"Unknown transfer ID must remain rejected");
  check(gpu.gpu(invalidScan)==0x1203,"Unknown scanout ID must remain rejected");
  check(gpu.gpu(invalidFlush)==0x1203,"Unknown flush ID must remain rejected");
  check(guest.display_rgb()==Bytes({0x11,0x22,0x33}),"Rejected IDs changed visible pixels");
  auto zeroTransfer=transfer;field(zeroTransfer,48,0,4);
  auto zeroFlush=flush;field(zeroFlush,40,0,4);
  check(gpu.gpu(zeroTransfer)==0x1203&&gpu.gpu(zeroFlush)==0x1203,"Reserved resource0 is not an unused table slot");
  auto disable=scan;field(disable,44,0,4);
  check(gpu.gpu(disable)==0x1100&&guest.display_rgb().empty(),"SET_SCANOUT resource0 must disable output");
  check(gpu.gpu(scan)==0x1100&&guest.display_rgb()==Bytes({0x11,0x22,0x33}),"Re-enable original scanout");
  auto remove=command(0x102,32);field(remove,24,42,4);
  check(gpu.gpu(remove)==0x1100&&guest.display_rgb().empty(),"Unref must retire scanout and resource");
  check(gpu.gpu(transfer)==0x1203&&gpu.gpu(flush)==0x1203,"Stale retired ID must remain rejected");
  field(create,24,43,4);field(attach,24,43,4);field(transfer,48,43,4);field(scan,44,43,4);field(flush,40,43,4);
  check(gpu.gpu(create)==0x1100&&gpu.gpu(attach)==0x1100&&gpu.gpu(transfer)==0x1100&&gpu.gpu(scan)==0x1100,"New ID after slot reuse");
  check(guest.machine().pci.resources[0].id==43&&guest.display_rgb()==Bytes({0x11,0x22,0x33}),"Slot reuse changed IDs or pixels");
  // The actual device reset retires resources. A successful observer must not
  // fabricate replacements to suppress the expected invalid-ID responses.
  wr(guest,gpu.bar+20,0,1);
  check(guest.machine().pci.resources[0].id==0&&guest.display_rgb().empty(),"Device reset lifetime");
  const auto final=guest.snapshot();
  check(MinimalGuest::from_snapshot_view(final,nullptr,MinimalGuest::max_view_snapshot).snapshot()==final,"Final original decoder roundtrip");
  save(argv[1],final);
  std::cout<<"{\"protocol_fixture_only\":true,\"checks\":"<<checks
    <<",\"checkpoint_sha256\":"<<quote(GuestSha256::hash(final))
    <<",\"original_decoder_roundtrip\":true,\"unknown_and_retired_ID_rejections\":7,\"android_boot_verified\":false}\n";
  return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; } }
