/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../native/cuttlefish-peripheral-protocol.h"
int main(void) {
  /* Independent literal vectors, not encoded using the implementation. */
  const ra_u8 get_carrier[]={0,0,0,0,0,0,0,0};
  const ra_u8 carrier_yes[]={0,0,0,128,1,0,0,0,1};
  const ra_u8 set_device_no[]={1,0,0,0,1,0,0,0,0};
  const ra_u8 device_no[]={1,0,0,128,1,0,0,0,0};
  struct ra_parser p={0};struct ra_message m={0};ra_u8 out[512],next=0;
  for(unsigned i=0;i<sizeof(get_carrier);i++)assert(ra_feed(&p,get_carrier[i],&m)==(i==7));
  assert(ra_oem(&m,3,&next,out)==9&&next==3&&!memcmp(out,carrier_yes,9));
  for(unsigned i=0;i<sizeof(set_device_no);i++)assert(ra_feed(&p,set_device_no[i],&m)==(i==8));
  assert(ra_oem(&m,3,&next,out)==9&&next==1&&!memcmp(out,device_no,9));
  m.command=2;m.size=0;
  for(unsigned state=0;state<8;state++){
    assert(ra_oem(&m,(ra_u8)state,&next,out)==9);
    assert(next==state&&out[8]==((state&3)==3));
  }
  m.command=3;m.size=1;m.payload[0]=1;assert(ra_oem(&m,3,&next,out)==9&&next==7&&out[8]==1);
  m.command=2;assert(ra_oem(&m,3,&next,out)==-1);
  m.command=0;m.payload[0]=2;assert(ra_oem(&m,3,&next,out)==-1);
  m.command=4;m.size=0;assert(ra_oem(&m,3,&next,out)==-1);
  m.command=0;assert(ra_oem(&m,8,&next,out)==-1);
  const ra_u8 oversized[]={0,0,0,0,1,1,0,0};
  const ra_u8 response_as_request[]={0,0,0,128,0,0,0,0};
  for(unsigned kind=0;kind<2;kind++) {
    memset(&p,0,sizeof(p));const ra_u8 *v=kind?response_as_request:oversized;
    for(unsigned i=0;i<8;i++)assert(ra_feed(&p,v[i],&m)==(i==7?-1:0));
  }
  /* Maximal fragmented request followed immediately by another header. */
  memset(&p,0,sizeof(p));ra_u8 large[264]={0};large[5]=1;
  for(unsigned i=0;i<sizeof(large);i++)assert(ra_feed(&p,large[i],&m)==(i==263));
  assert(m.size==256);
  for(unsigned i=0;i<8;i++)assert(ra_feed(&p,get_carrier[i],&m)==(i==7));
  m.command=0;m.size=12;memcpy(m.payload,"list-sensors",12);
  const ra_u8 sensor_list[]={2,0,0,128,4,0,0,0,'5','0','3','\n'};
  assert(ra_sensor_list(&m,out)==12&&!memcmp(out,sensor_list,12));
  m.size=11;assert(!ra_sensor_list(&m,out));
  m.size=12;m.payload[0]='X';assert(!ra_sensor_list(&m,out));
  unsigned continuous=0;for(unsigned i=0;i<8;i++)continuous+=ra_sensor_continuous(i);
  assert(continuous==5&&RA_SENSOR_MASK==503);
  puts("{\"literal_wire_vectors\":true,\"fragmented_and_coalesced_requests\":true,\"state_truth_table\":true,\"bounded_malformed_input\":true,\"separate_sensor_contract\":true}");
  return 0;
}
