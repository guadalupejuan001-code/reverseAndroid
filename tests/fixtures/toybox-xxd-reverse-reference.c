/* Upstream Toybox xxd reverse algorithm, Copyright 2015 The Android Open Source Project.
 * Source: https://github.com/landley/toybox/blob/master/toys/other/xxd.c
 * Upstream whole-file Git blob SHA1: 2a82d63f85fe85709c1166238b61898dba794f07
 * The dehex and do_xxd_reverse bodies below are unmodified. Test harness only.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#define FLAG(x) 0
static struct { long c; } TT={16};
static int xdup(int fd){int r=dup(fd);if(r<0){perror("dup");exit(1);}return r;}
static FILE *xfdopen(int fd,const char *mode){FILE *f=fdopen(fd,mode);if(!f){perror("fdopen");exit(1);}return f;}
static void error_exit(const char *fmt,...){va_list a;va_start(a,fmt);vfprintf(stderr,fmt,a);va_end(a);exit(1);}
#define perror_exit error_exit
static void perror_msg_raw(const char *s){perror(s);}
static void xputc(int c){putchar(c);}
static int dehex(char ch)
{
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return (ch == '\n') ? -2 : -1;
}

static void do_xxd_reverse(int fd, char *name)
{
  FILE *fp = xfdopen(xdup(fd), "r");
  long long pos, current_pos = 0;
  int tmp;

  // -ri is a very easy special case.
  if (FLAG(i)) while (fscanf(fp, " 0x%02x,", &tmp) == 1) xputc(tmp);
  else while (!feof(fp)) {
    int col = 0;
    char ch;

    // Each line of a regular hexdump starts with an offset/address.
    // Each line of a plain hexdump just goes straight into the bytes.
    if (!FLAG(p) && fscanf(fp, "%llx%c ", &pos, &ch) == 2) {
      if (ch != ':' && ch != ' ')
        error_exit("%s: no separator between offset/address and bytes "
            "(missing -p?)", name);
      if (pos != current_pos && fseek(stdout, pos, SEEK_SET)) {
        // TODO: just write out zeros if non-seekable?
        perror_exit("%s: seek %llx failed", name, pos);
      }
    }

    // A plain hexdump can have as many bytes per line as you like,
    // but a non-plain hexdump assumes garbage after it's seen the
    // specified number of bytes.
    while (FLAG(p) || !TT.c || col < TT.c) {
      int n1, n2;

      // If we're at EOF or EOL or we read some non-hex...
      if ((n1 = n2 = dehex(fgetc(fp))) < 0 || (n2 = dehex(fgetc(fp))) < 0) {
        // If we're at EOL, start on that line.
        if (n1 == -2 || n2 == -2) continue;
        // Otherwise, skip to the next line.
        break;
      }

      fputc((n1 << 4) | (n2 & 0xf), stdout);
      col++;
      current_pos++;

      // Is there any grouping going on? Ignore a single space.
      tmp = fgetc(fp);
      if (tmp != ' ') ungetc(tmp, fp);
    }

    // Skip anything else on this line (such as the ASCII dump).
    while ((tmp = fgetc(fp)) != EOF && tmp != '\n');
  }

  if (ferror(fp)) perror_msg_raw(name);
  fclose(fp);
}

int main(int argc,char **argv){if(argc!=2||strcmp(argv[1],"-r"))return 2;do_xxd_reverse(0,"stdin");return fflush(stdout)||ferror(stdout);}
