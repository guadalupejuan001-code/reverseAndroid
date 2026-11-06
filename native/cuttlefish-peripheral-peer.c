/* SPDX-License-Identifier: Apache-2.0 */
/* Real Linux PTY streams for original Cuttlefish HAL executables. No Binder
 * registration, snapshot edits, property writes, SELinux relaxation or clocks
 * from the emulator host. AArch64 build is static and libc independent. */
#include "cuttlefish-peripheral-protocol.h"
#include "cuttlefish-peripheral-linux-abi.h"
#if defined(RA_HOST_TEST)
#define _GNU_SOURCE 1
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
typedef struct stat ra_stat;
#define NR_openat SYS_openat
#define NR_close SYS_close
#define NR_read SYS_read
#define NR_write SYS_write
#define NR_ioctl SYS_ioctl
#define NR_fcntl SYS_fcntl
#define NR_fstat SYS_fstat
#define NR_fchmod SYS_fchmod
#define NR_fchown SYS_fchown
#define NR_fsetxattr SYS_fsetxattr
#define NR_fgetxattr SYS_fgetxattr
#define NR_lsetxattr SYS_lsetxattr
#define NR_lgetxattr SYS_lgetxattr
#define NR_mkdirat SYS_mkdirat
#define NR_symlinkat SYS_symlinkat
#define NR_unlinkat SYS_unlinkat
#define NR_lseek SYS_lseek
#define NR_fsync SYS_fsync
#define NR_ppoll SYS_ppoll
#define NR_clock_gettime SYS_clock_gettime
#define NR_geteuid SYS_geteuid
#define NR_exit_group SYS_exit_group
static long call(long n,long a,long b,long c,long d,long e,long f) {
  long r=syscall(n,a,b,c,d,e,f); return r==-1?-errno:r;
}
#else
#if !defined(__aarch64__)
#error Use AArch64 target or RA_HOST_TEST
#endif
typedef struct {
  unsigned long dev,ino; unsigned st_mode,nlink,st_uid,st_gid;
  unsigned long rdev,pad1; long size; int blksize,pad2; long blocks;
  long atime_sec,atime_nsec,mtime_sec,mtime_nsec,ctime_sec,ctime_nsec;
  unsigned unused[2];
} ra_stat;
_Static_assert(sizeof(ra_stat)==128,"asm-generic stat layout");
#define NR_openat 56
#define NR_close 57
#define NR_read 63
#define NR_write 64
#define NR_ioctl 29
#define NR_fcntl 25
#define NR_fstat 80
#define NR_fchmod 52
#define NR_fchown 55
#define NR_fsetxattr 7
#define NR_fgetxattr 10
#define NR_lsetxattr 6
#define NR_lgetxattr 9
#define NR_mkdirat 34
#define NR_symlinkat 36
#define NR_unlinkat 35
#define NR_lseek 62
#define NR_fsync 82
#define NR_ppoll 73
#define NR_clock_gettime 113
#define NR_geteuid 175
#define NR_exit_group 94
void *memcpy(void *dst,const void *src,unsigned long size) {
  volatile ra_u8 *d=dst;const volatile ra_u8 *s=src;for(unsigned long i=0;i<size;i++)d[i]=s[i];return dst;
}
void *memset(void *dst,int value,unsigned long size) {
  volatile ra_u8 *d=dst;for(unsigned long i=0;i<size;i++)d[i]=(ra_u8)value;return dst;
}
static long call(long n,long a,long b,long c,long d,long e,long f) {
  register long x0 __asm__("x0")=a,x1 __asm__("x1")=b,x2 __asm__("x2")=c;
  register long x3 __asm__("x3")=d,x4 __asm__("x4")=e,x5 __asm__("x5")=f;
  register long x8 __asm__("x8")=n;
  __asm__ volatile("svc #0":"+r"(x0):"r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5),"r"(x8):"memory","cc");
  return x0;
}
#endif
#define PTR(x) ((long)(x))
#define C0(n) call(NR_##n,0,0,0,0,0,0)
#define C1(n,a) call(NR_##n,(long)(a),0,0,0,0,0)
#define C2(n,a,b) call(NR_##n,(long)(a),(long)(b),0,0,0,0)
#define C3(n,a,b,c) call(NR_##n,(long)(a),(long)(b),(long)(c),0,0,0)
#define C4(n,a,b,c,d) call(NR_##n,(long)(a),(long)(b),(long)(c),(long)(d),0,0)
#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif
#if !defined(RA_HOST_TEST)
#define O_RDWR RA_ARM64_O_RDWR
#define O_CREAT RA_ARM64_O_CREAT
#define O_EXCL RA_ARM64_O_EXCL
#define O_NOCTTY RA_ARM64_O_NOCTTY
#define O_NONBLOCK RA_ARM64_O_NONBLOCK
#define O_DIRECTORY RA_ARM64_O_DIRECTORY
#define O_NOFOLLOW RA_ARM64_O_NOFOLLOW
#define O_CLOEXEC RA_ARM64_O_CLOEXEC
#endif
#define EAGAIN 11
#define EINTR 4
#define ENOENT 2
#define EEXIST 17
static unsigned slen(const char *s) { unsigned n=0;while(s[n])n++;return n; }
static int equal(const char *a,const char *b) { unsigned i=0;while(a[i]&&a[i]==b[i])i++;return a[i]==b[i]; }
static void copy(ra_u8 *a,const ra_u8 *b,unsigned n) { for(unsigned i=0;i<n;i++)a[i]=b[i]; }
static long putall(int fd,const void *buf,unsigned n) {
  unsigned done=0;while(done<n){long k=C3(write,fd,PTR((const ra_u8*)buf+done),n-done);if(k==-EINTR)continue;if(k<=0)return k?k:-5;done+=(unsigned)k;}return done;
}
static void logline(const char *s) { putall(2,s,slen(s));putall(2,"\n",1); }
static unsigned decimal(char *out,unsigned long n) {
  char rev[24];unsigned k=0;do{rev[k++]=(char)('0'+n%10);n/=10;}while(n);
  for(unsigned i=0;i<k;i++)out[i]=rev[k-i-1];out[k]=0;return k;
}
static void numberlog(const char *prefix,long v) {
  char b[32];putall(2,prefix,slen(prefix));if(v<0){putall(2,"-",1);v=-v;}unsigned n=decimal(b,(unsigned long)v);putall(2,b,n);putall(2,"\n",1);
}
struct timespec64 { long sec,nsec; };
static long long monotonic(void) {
  struct timespec64 t; if(C2(clock_gettime,1,PTR(&t))<0)return -1;
  return (long long)t.sec*1000000000LL+t.nsec;
}
struct channel {
  int master,slave;const char *alias;unsigned published;
  struct ra_parser parser;ra_u8 out[1024];unsigned sent,count;
};
static struct channel channels[3];
static const char *const aliases[3]={"/dev/hvc10","/dev/hvc18","/dev/hvc19"};
static const char *const labels[3]={"u:object_r:oemlock_device:s0","u:object_r:sensors_device:s0","u:object_r:sensors_device:s0"};
static const unsigned owners[3]={1064,1000,1000};
static int state_fd=-1;
static ra_u8 state_byte;
static int label_fd(int fd,const char *label) {
  const char *key="security.selinux";char b[128];unsigned n=slen(label)+1;
  long r=C4(fsetxattr,fd,PTR(key),PTR(label),n);
  if(r<0){numberlog("SELINUX_SET_FD_ERRNO ",r);return -1;}
  long k=C4(fgetxattr,fd,PTR(key),PTR(b),sizeof(b));
  if(k<0)numberlog("SELINUX_GET_FD_ERRNO ",k);
  if(k!=(long)n)numberlog("SELINUX_FD_CONTEXT_BYTES ",k);
  return k==(long)n&&equal(b,label)?0:-1;
}
static int label_alias(const char *path) {
  const char *key="security.selinux",*label="u:object_r:device:s0";char b[128];unsigned n=slen(label)+1;
  long r=C4(lsetxattr,PTR(path),PTR(key),PTR(label),n);
  if(r<0){numberlog("SELINUX_SET_ALIAS_ERRNO ",r);return -1;}
  long k=C4(lgetxattr,PTR(path),PTR(key),PTR(b),sizeof(b));
  if(k!=(long)n)numberlog("SELINUX_ALIAS_CONTEXT_BYTES ",k);
  return k==(long)n&&equal(b,label)?0:-1;
}
static int provision(unsigned i) {
  struct channel *c=channels+i;int unlocked=0;unsigned index;
  c->master=(int)C4(openat,AT_FDCWD,PTR("/dev/ptmx"),O_RDWR|O_NOCTTY|O_NONBLOCK|O_CLOEXEC,0);
  if(c->master<0){numberlog("PTMX_OPEN_ERRNO ",c->master);return -1;}
  if(C3(ioctl,c->master,0x40045431,PTR(&unlocked))<0 || C3(ioctl,c->master,0x80045430,PTR(&index))<0)return -1;
  char path[64]="/dev/pts/";decimal(path+9,index);
  c->slave=(int)C4(openat,AT_FDCWD,PTR(path),O_RDWR|O_NOCTTY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC,0);
  if(c->slave<0)return -1;
  struct { unsigned iflag,oflag,cflag,lflag;ra_u8 line,cc[19]; } term;
  _Static_assert(sizeof(term)==36,"Linux kernel termios");
  if(C3(ioctl,c->slave,0x5401,PTR(&term))<0)return -1;
  term.iflag=term.oflag=term.lflag=0;term.cflag=(term.cflag&~(48u|256u))|48u|128u|2048u;term.cc[5]=0;term.cc[6]=1;
  if(C3(ioctl,c->slave,0x5402,PTR(&term))<0)return -1;
  if(C3(fchown,c->slave,owners[i],owners[i])<0 || C2(fchmod,c->slave,0600)<0 || label_fd(c->slave,labels[i])<0)return -1;
  long alias_result=C3(symlinkat,PTR(path),AT_FDCWD,PTR(aliases[i]));
  if(alias_result<0){numberlog("ALIAS_CREATE_ERRNO ",alias_result);return -1;}
  c->alias=aliases[i];c->published=1;
  if(label_alias(c->alias)<0)return -1;
  logline(c->alias);logline(path);logline(labels[i]);return 0;
}
static void cleanup(void) {
  for(unsigned i=0;i<3;i++) {
    struct channel *c=channels+i;
    if(c->published)C3(unlinkat,AT_FDCWD,PTR(c->alias),0);
    if(c->master>=0)C1(close,c->master);
    if(c->slave>=0)C1(close,c->slave);
  }
  if(state_fd>=0)C1(close,state_fd);
}
static int persist(ra_u8 value) {
  if(C3(lseek,state_fd,0,0)<0 || putall(state_fd,&value,1)!=1 || C1(fsync,state_fd)<0)return -1;
  state_byte=value;return 0;
}
static int open_state(int dir,const char *name,unsigned uid) {
  int created=0;
  state_fd=(int)C4(openat,dir,PTR(name),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
  if(state_fd>=0)created=1;
  else if(state_fd==-EEXIST)state_fd=(int)C4(openat,dir,PTR(name),O_RDWR|O_NOFOLLOW|O_CLOEXEC,0);
  if(state_fd<0)return -1;
  ra_stat st;if(C2(fstat,state_fd,PTR(&st))<0 || st.st_uid!=uid || (st.st_mode&0177777u)!=0100600u)return -1;
  /* Upstream software OEM peripheral's documented initial state is byte3. */
  if(created)return persist(3);
  ra_u8 b[2];long k=C3(read,state_fd,PTR(b),2);if(k!=1 || (b[0]&~7u))return -1;
  state_byte=b[0];return 0;
}
static int guest_state(void) {
  const char *path="/data/local/tmp/reverseandroid-peripherals";
  long k=C3(mkdirat,AT_FDCWD,PTR(path),0700);if(k<0&&k!=-EEXIST)return -1;
  int dir=(int)C4(openat,AT_FDCWD,PTR(path),O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,0);
  if(dir<0){numberlog("STATE_DIRECTORY_OPEN_ERRNO ",dir);return -1;}
  ra_stat st;k=C2(fstat,dir,PTR(&st));
  int ok=k>=0&&st.st_uid==0&&(st.st_mode&0177777u)==0040700u;
  int r=ok?open_state(dir,"oemlock-state",0):-1;C1(close,dir);return r;
}
static int enqueue(struct channel *c,const ra_u8 *data,unsigned n) {
  if(c->count+n>sizeof(c->out))return -1;copy(c->out+c->count,data,n);c->count+=n;return 0;
}
static int flush(struct channel *c) {
  while(c->sent<c->count) {
    long k=C3(write,c->master,PTR(c->out+c->sent),c->count-c->sent);
    if(k==-EINTR)continue;if(k==-EAGAIN)return 0;if(k<=0)return -1;c->sent+=(unsigned)k;
  }
  c->sent=c->count=0;return 0;
}
static unsigned activated;
static int request(unsigned i,const struct ra_message *m) {
  ra_u8 out[512];int n;
  if(i==0) {
    ra_u8 next;n=ra_oem(m,state_byte,&next,out);if(n<0)return -1;
    if(next!=state_byte && persist(next)<0)return -1;
    numberlog("OEM_REQUEST_COMMAND ",m->command);numberlog("OEM_RESPONSE_BOOL ",out[8]);
  } else if(i==1) {
    n=ra_sensor_list(m,out);
    if(n) { activated=1;logline("SENSORS_LIST_REQUEST mask=503"); }
    else { numberlog("SENSORS_OTHER_REQUEST_BYTES ",m->size);return 0; }
  } else return -1;
  return enqueue(channels+i,out,(unsigned)n);
}
static int receive(unsigned i) {
  struct channel *c=channels+i;ra_u8 bytes[256];struct ra_message m;
  long n=C3(read,c->master,PTR(bytes),sizeof(bytes));
  if(n==-EAGAIN||n==-EINTR)return 0;if(n<=0)return -1;
  for(long k=0;k<n;k++){int r=ra_feed(&c->parser,bytes[k],&m);if(r<0||(r&&request(i,&m)<0))return -1;}return 0;
}
static int sensors(unsigned initial) {
  struct channel *c=channels+2;if(c->count)return 0;
  ra_u8 out[128];for(unsigned i=0;i<8;i++) {
    if(!initial&&!ra_sensor_continuous(i))continue;
    unsigned n=ra_response(out,2,(const ra_u8*)ra_sensor_samples[i],slen(ra_sensor_samples[i]));
    if(enqueue(c,out,n)<0)return -1;
  }
  logline(initial?"SENSORS_INITIAL_STATIONARY_REPORT":"SENSORS_CONTINUOUS_REPORT");return 1;
}
static int serve(void) {
  struct { int fd;short events,revents; } fds[3];
  long long deadline=0;unsigned first=1;
  for(;;) {
    long long now=monotonic();if(now<0)return -1;
    if(activated&&now>=deadline) {
      int r=sensors(first);if(r<0)return -1;if(r>0)first=0;deadline=now+1000000000LL;
    }
    for(unsigned i=0;i<3;i++) {
      if(flush(channels+i)<0)return -1;
      fds[i].fd=channels[i].master;fds[i].events=(short)((i<2?1:0)|(channels[i].count?4:0));fds[i].revents=0;
    }
    struct timespec64 timeout={1,0};
    if(activated){long long left=deadline-now;if(left<0)left=0;timeout.sec=(long)(left/1000000000LL);timeout.nsec=(long)(left%1000000000LL);}
    long k=call(NR_ppoll,PTR(fds),3,PTR(&timeout),0,8,0);
    if(k==-EINTR)continue;if(k<0)return -1;
    for(unsigned i=0;i<3;i++) {
      if(fds[i].revents&(8|16|32))return -1;
      if((fds[i].revents&1)&&receive(i)<0)return -1;
      if((fds[i].revents&4)&&flush(channels+i)<0)return -1;
    }
  }
}
static int run(int argc,char **argv) {
  for(unsigned i=0;i<3;i++)channels[i].master=channels[i].slave=-1;
  if(argc==2&&equal(argv[1],"--guest")) {
#if defined(RA_HOST_TEST)
    logline("Guest provisioning disabled in host-test executable");return 1;
#endif
    if(C0(geteuid)!=0){logline("Root required");return 1;}
    if(guest_state()<0){logline("State ownership, mode or persistence failed");cleanup();return 1;}
    for(unsigned i=0;i<3;i++)if(provision(i)<0){numberlog("PTY_PROVISION_FAILED_CHANNEL ",i);cleanup();return 1;}
  }
#if defined(RA_HOST_TEST)
  else if(argc==6&&equal(argv[1],"--host-fds")) {
    for(unsigned i=0;i<3;i++) {
      const char *s=argv[i+2];unsigned v=0;if(!*s)return 1;
      for(;*s;s++){if(*s<'0'||*s>'9'||v>100000)return 1;v=v*10+(unsigned)(*s-'0');}
      channels[i].master=(int)v;
      long flags=C3(fcntl,v,3,0);if(flags<0||C3(fcntl,v,4,flags|O_NONBLOCK)<0)return 1;
    }
    if(open_state(AT_FDCWD,argv[5],(unsigned)C0(geteuid))<0){logline("Host-test state failed");cleanup();return 1;}
  }
#endif
  else {logline("Usage: cuttlefish-peripheral-peer --guest");return 1;}
  logline("REVERSE_PERIPHERAL_PEER_READY");int r=serve();numberlog("PERIPHERAL_STOP ",r);cleanup();return 1;
}
#if defined(RA_HOST_TEST)
int main(int argc,char **argv) {return run(argc,argv);}
#else
void ra_start(unsigned long *stack) {
  int r=run((int)stack[0],(char**)(stack+1));C1(exit_group,r);for(;;){}
}
__asm__(".global _start\n.type _start,%function\n_start:\nmov x0,sp\nbl ra_start\n");
#endif
