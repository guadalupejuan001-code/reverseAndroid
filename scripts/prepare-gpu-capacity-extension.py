#!/usr/bin/env python3
"""Explicit 16-resource device profile; preserve the four-resource format verbatim."""
import hashlib
import json
import pathlib
import subprocess
import sys

root=pathlib.Path(__file__).resolve().parents[1]
out=pathlib.Path(sys.argv[1]).resolve()
subprocess.run([sys.executable,str(root/'scripts/prepare-ic-ivau-extension.py'),str(out)],check=True)
h=lambda b:hashlib.sha256(b).hexdigest()
prior=json.loads((out/'extension-manifest.json').read_text())
changes={}
def patch(name,replacements):
    path=out/name;before=path.read_text();after=before
    for old,new in replacements:
        if after.count(old)!=1:raise RuntimeError('Ambiguous GPU capacity anchor: '+name)
        after=after.replace(old,new)
    path.write_text(after);changes[name]={'before_sha256':h(before.encode()),'after_sha256':h(after.encode())}

patch('minimal_guest_pci_state.hpp',[
    ('std::array<GuestGpuResource,4> resources{};',
     'std::array<GuestGpuResource,16> resources{};'),
    ('    put(1);put(enabled);put(disk_bytes);put(flushes);',
     '    bool extended=false;std::size_t entries=0;\n'
     '    for(unsigned i=0;i<resources.size();++i){const auto& r=resources[i];entries+=r.backing.size();if(i>=4&&(r.id||r.format||r.width||r.height||!r.backing.empty()))extended=true;}\n'
     '    if(entries>8192)throw std::runtime_error("GPU aggregate backing metadata budget");\n'
     '    put(extended?2:1);put(enabled);put(disk_bytes);put(flushes);'),
    ('    for(auto& r:resources){put(r.id);',
     '    const unsigned count=extended?16:4;if(extended)put(count);\n'
     '    for(unsigned index=0;index<count;++index){const auto& r=resources[index];put(r.id);'),
    ('    if(take()!=1)throw std::runtime_error("Unsupported PCI state version");GuestPciState p;',
     '    const auto version=take();if(version!=1&&version!=2)throw std::runtime_error("Unsupported PCI state version");GuestPciState p;'),
    ('    for(auto& r:p.resources){r.id=small(UINT32_MAX);',
     '    const auto count=version==1?4:small(16);if(version==2&&count!=16)throw std::runtime_error("Unsupported extended GPU capacity");\n'
     '    std::size_t total_entries=0;\n'
     '    for(unsigned index=0;index<count;++index){auto& r=p.resources[index];r.id=small(UINT32_MAX);'),
    ('const auto n=small(2048);if(!r.id',
     'const auto n=small(2048);total_entries+=n;if(total_entries>8192)throw std::runtime_error("GPU aggregate backing metadata budget");if(!r.id'),
    ('    if(at!=b.size())throw std::runtime_error("Trailing PCI state");return p;',
     '    if(at!=b.size())throw std::runtime_error("Trailing PCI state");\n'
     '    if(version==2&&p.save()!=b)throw std::runtime_error("Noncanonical extended GPU state");return p;'),
])
patch('minimal_guest_pci.inc',[
    ('if(total<std::uint64_t(r.width)*r.height*4)result=0x1205;else r.backing=std::move(entries);',
     'std::size_t other_entries=0;for(unsigned k=0;k<p.resources.size();++k)if(k!=unsigned(i))other_entries+=p.resources[k].backing.size();\n'
     '        if(total<std::uint64_t(r.width)*r.height*4)result=0x1205;else if(other_entries+entries.size()>8192)result=0x1201;else r.backing=std::move(entries);'),
])
patch('minimal_guest.hpp',[
    ('  Bytes display_rgb() const;',
     '  Bytes display_rgb() const;\n'
     '  // Explicit bounded projection for the CPU/QSSD/GPU bridge. No measurement.\n'
     '  std::uint32_t gpu_resource_word(unsigned id,std::uint64_t index) const {\n'
     '    require(id!=0,"Reserved GPU resource ID");\n'
     '    for(unsigned slot=0;slot<machine_.pci.resources.size();++slot){const auto& r=machine_.pci.resources[slot];if(r.id!=id)continue;\n'
     '      require(index<std::uint64_t(r.width)*r.height,"GPU resource word outside extent");\n'
     '      std::uint32_t result=0;for(unsigned c=0;c<4;++c)result|=std::uint32_t(logical_device_byte(GuestPciState::gpu_pixels+(std::uint64_t(slot)<<24)+4*index+c))<<(8*c);return result;\n'
     '    }throw std::runtime_error("Unknown GPU resource ID");\n'
     '  }\n'
     '  const void* gpu_bridge_image_identity() const noexcept {return image_.get();}'),
])
manifest={'schema':'reverseandroid-explicit-GPU-capacity-profile-v1',
          'retained_ISA_extension':prior,'resource_capacity':16,
          'original_resource_capacity':4,'aggregate_backing_entry_budget':8192,
          'GPU_pixel_slot_bytes':16777216,'scanout_plane_offset':268435456,
          'new_GPU_PCI_subformat_version':2,'legacy_GPU_PCI_subformat_1_bytes_preserved':True,
          'legacy_GPU_PCI_subformat_1_decoder_preserved':True,
          'old_native_cannot_read_new_GPU_PCI_subformat_2':True,
          'RVG_MXIM_RFP_wrapper_formats_unchanged':True,
          'all_870_imported_files_unchanged':True,'modified_build_copy_files':changes,
          'RAM_disk_clock_seed_cadence_IRQ_and_security_gates_unchanged':True,
          'new_GPU_or_QSSD_renderer_acceleration_claimed':False}
(out/'gpu-capacity-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
subprocess.run([sys.executable,str(root/'scripts/verify-reverseandroid-import.py')],check=True)
print(json.dumps(manifest))
