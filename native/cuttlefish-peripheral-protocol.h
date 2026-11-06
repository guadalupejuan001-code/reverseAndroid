/* SPDX-License-Identifier: Apache-2.0 */
/* Wire contract: google/android-cuttlefish@55d6954ef1fbbadea7b28d9ba19b902d3fc08aac.
 * This is a peripheral peer; it does not implement or register an Android HAL. */
#ifndef RA_CUTTLEFISH_PERIPHERAL_PROTOCOL_H
#define RA_CUTTLEFISH_PERIPHERAL_PROTOCOL_H
typedef unsigned char ra_u8;
typedef unsigned int ra_u32;
_Static_assert(sizeof(ra_u32) == 4, "32-bit wire words required");
#define RA_MAX_PAYLOAD 256u
#define RA_SENSOR_MASK 503u
struct ra_message { ra_u32 command, size; ra_u8 payload[RA_MAX_PAYLOAD]; };
struct ra_parser { ra_u8 bytes[8 + RA_MAX_PAYLOAD]; unsigned used, wanted; };
static ra_u32 ra_get32(const ra_u8 *p) {
  return (ra_u32)p[0] | ((ra_u32)p[1]<<8) | ((ra_u32)p[2]<<16) | ((ra_u32)p[3]<<24);
}
static void ra_put32(ra_u8 *p, ra_u32 v) {
  for (unsigned i=0;i<4;i++) p[i]=(ra_u8)(v>>(8*i));
}
/* One byte at a time makes every transport partition equivalent. -1 = invalid. */
static int ra_feed(struct ra_parser *p, ra_u8 b, struct ra_message *m) {
  if (!p->wanted) p->wanted=8;
  if (p->used>=sizeof(p->bytes)) return -1;
  p->bytes[p->used++]=b;
  if (p->used==8) {
    ra_u32 command=ra_get32(p->bytes), size=ra_get32(p->bytes+4);
    if ((command&0x80000000u) || size>RA_MAX_PAYLOAD) return -1;
    p->wanted=8+size;
  }
  if (p->used!=p->wanted) return 0;
  m->command=ra_get32(p->bytes); m->size=ra_get32(p->bytes+4);
  for (unsigned i=0;i<m->size;i++) m->payload[i]=p->bytes[8+i];
  p->used=0; p->wanted=8; return 1;
}
static unsigned ra_response(ra_u8 *out, ra_u32 command, const ra_u8 *body, unsigned size) {
  ra_put32(out,command|0x80000000u); ra_put32(out+4,size);
  for (unsigned i=0;i<size;i++) out[8+i]=body[i];
  return 8+size;
}
/* Calculate a prospective state. The runtime must persist it BEFORE replying.
 * bit0 carrier allowed, bit1 device allowed, bit2 locked. Allowed is their AND,
 * independent of locked, exactly as the upstream software peripheral. */
static int ra_oem(const struct ra_message *m, ra_u8 state, ra_u8 *next, ra_u8 *reply) {
  if ((state&~7u) || m->command>3 || m->size>1 ||
      (m->command==2 && m->size) || (m->size && m->payload[0]>1)) return -1;
  unsigned bit=m->command==3?2:m->command;
  *next=state;
  if (m->size) *next=(ra_u8)((state&~(1u<<bit)) | (m->payload[0]<<bit));
  ra_u8 value=m->command==2 ? ((*next&3u)==3u) : ((*next>>bit)&1u);
  return (int)ra_response(reply,m->command,&value,1);
}
static int ra_sensor_list(const struct ra_message *m, ra_u8 *reply) {
  static const char query[]="list-sensors";
  static const ra_u8 mask[]={'5','0','3','\n'};
  if (m->size<sizeof(query)-1) return 0;
  for (unsigned i=0;i<sizeof(query)-1;i++) if(m->payload[i]!=(ra_u8)query[i]) return 0;
  return (int)ra_response(reply,2,mask,sizeof(mask));
}
/* Explicit stationary virtual phone, flat with +Z up. SI acceleration/angular
 * velocity, microtesla, deg C, cm, lux, hPa and %RH. These are physical-model
 * inputs, never Android properties or fabricated display/boot results. */
static const char *const ra_sensor_samples[8]={
  "acceleration:0:0:9.80665\n", "gyroscope:0:0:0\n", "magnetic:0:5.9:-48.1\n",
  "temperature:25\n", "proximity:5\n", "light:100\n", "pressure:1013.25\n", "humidity:45\n"
};
/* First report includes all eight; following reports include continuous IDs
 * 0,1,2,6,7 (mask199), matching the phone intersection in upstream. */
static int ra_sensor_continuous(unsigned i) { return i<3 || i==5 || i==6; }
#endif
