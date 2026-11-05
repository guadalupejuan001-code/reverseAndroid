#include "minimal_guest_machine.hpp"
#include <chrono>
#include <iostream>
using namespace reverse_pc::guest;
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok)throw std::runtime_error("IRQ selector differs from original");}
static void equal(const MachineState& m){check(m.next_irq()==m.next_irq_reference());}
static void bit(std::array<std::uint32_t,2>& words,unsigned i){words[i/32]|=1u<<(i%32);}
int main(){try{
 MachineState base;base.enabled=true;base.dist_ctl=base.cpu_ctl=1;
 equal(base);check(base.next_irq()==1023);
 for(unsigned id=0;id<64;++id){
  auto m=base;bit(m.enabled_irqs,id);bit(m.pending_irqs,id);equal(m);check(m.next_irq()==id);
  auto before=m.save();for(unsigned n=0;n<8;++n)equal(m);check(m.save()==before);
  bit(m.active_irqs,id);equal(m);check(m.next_irq()==1023);
  m=base;bit(m.enabled_irqs,id);bit(m.pending_irqs,id);m.targets[id]=0;equal(m);check(m.next_irq()==(id<32?id:1023));
  m.priority[id]=m.pmr;equal(m);check(m.next_irq()==1023);
  for(auto control:{0u,2u}){m=base;bit(m.enabled_irqs,id);bit(m.pending_irqs,id);m.dist_ctl=control;equal(m);check(m.next_irq()==1023);m.dist_ctl=1;m.cpu_ctl=control;equal(m);check(m.next_irq()==1023);}
 }
 for(unsigned a=0;a<64;++a)for(unsigned b=0;b<64;++b){
  auto m=base;bit(m.enabled_irqs,a);bit(m.enabled_irqs,b);bit(m.pending_irqs,a);bit(m.pending_irqs,b);equal(m);check(m.next_irq()==std::min(a,b));
  m.priority[b]=0;equal(m);check(m.next_irq()==b);
  bit(m.active_irqs,b);equal(m);check(m.next_irq()==1023);
 }
 for(unsigned ticks=0;ticks<4;++ticks)for(unsigned p=0;p<8;++p)for(unsigned v=0;v<8;++v){
  auto m=base;m.enabled_irqs.fill(UINT32_MAX);m.ticks=ticks;m.cntp_cval=2;m.cntv_cval=3;m.cntp_ctl=p;m.cntv_ctl=v;equal(m);
  m.receive="x";m.uart_imsc=16;equal(m);m.priority[33]=0;equal(m);
 }
 std::uint64_t seed=49;auto random=[&](){seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;return seed;};
 for(unsigned trial=0;trial<100000;++trial){
  auto m=base;m.dist_ctl=random()&3;m.cpu_ctl=random()&3;m.pmr=random()&255;
  for(unsigned w=0;w<2;++w){m.enabled_irqs[w]=random();m.pending_irqs[w]=random();m.active_irqs[w]=random();}
  for(unsigned n=0;n<64;++n){m.priority[n]=random()&255;m.targets[n]=random()&255;}
  m.ticks=random();m.cntp_cval=random();m.cntv_cval=random();m.cntp_ctl=random()&7;m.cntv_ctl=random()&7;
  if(random()&1)m.receive="irq";m.uart_imsc=random()&0x7ff;
  m.pci.enabled=random()&1;m.pci.disk_bytes=(random()&1)?512:0;
  for(auto& d:m.pci.devices){d.isr=random()&3;d.command=(random()&1)?1024:0;}
  equal(m);
 }
 // The boot hot case has no active interrupt and no asserted level yet.
 auto m=base;m.enabled_irqs[0]=0x08000fff;m.cntv_ctl=1;m.cntv_cval=1000000;
 constexpr unsigned rounds=1000000;volatile unsigned sink=0;
 auto start=std::chrono::steady_clock::now();
 for(unsigned n=0;n<rounds;++n){m.ticks=n;sink^=m.next_irq_reference();}
 auto old_us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 start=std::chrono::steady_clock::now();
 for(unsigned n=0;n<rounds;++n){m.ticks=n;sink^=m.next_irq();}
 auto new_us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
 std::cout<<"{\"checks\":"<<checks<<",\"random_states\":100000,\"original_us\":"<<old_us<<",\"candidate_us\":"<<new_us<<",\"selector_identical\":true,\"every_instruction_irq_checks_retained\":true,\"sink\":"<<sink<<"}\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
