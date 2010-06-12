#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "strerr.h"
#include "error.h"
#include "open.h"
#include "fmt.h"
#include "tai.h"
#include "buffer.h"
#include "svpath.h"

#define FATAL "svstat: fatal: "
#define WARNING "svstat: warning: "

char bspace[1024];
buffer b = BUFFER_INIT(buffer_unixwrite,1,bspace,sizeof bspace);

char status[18];
char strnum[FMT_ULONG];

unsigned long pid;
unsigned char normallyup;
unsigned char want;
unsigned char paused;

static void die_nomem(void)
{
  strerr_die2sys(111,FATAL,"unable to allocate memory");
}

void doit(char *dir)
{
  struct stat st;
  int r;
  int fd;
  const char *x;
  struct tai when;
  struct tai now;
  const char *fntemp;

  buffer_puts(&b,dir);
  buffer_puts(&b,": ");

  if (chdir(dir) == -1) {
    x = error_str(errno);
    buffer_puts(&b,"unable to chdir: ");
    buffer_puts(&b,x);
    return;
  }
  if (!svpath_init()) {
    strerr_warn4(WARNING,"unable to set up control path for ",dir,": ",&strerr_sys);
    return;
  }

  normallyup = 0;
  if (stat("down",&st) == -1) {
    if (errno != error_noent) {
      x = error_str(errno);
      buffer_puts(&b,"unable to stat down: ");
      buffer_puts(&b,x);
      return;
    }
    normallyup = 1;
  }

  if ((fntemp = svpath_make("/ok")) == 0) die_nomem();
  fd = open_write(fntemp);
  if (fd == -1) {
    if (errno == error_nodevice) {
      buffer_puts(&b,"supervise not running");
      return;
    }
    x = error_str(errno);
    buffer_puts(&b,"unable to open ");
    buffer_puts(&b,fntemp);
    buffer_puts(&b,": ");
    buffer_puts(&b,x);
    return;
  }
  close(fd);

  if ((fntemp = svpath_make("/status")) == 0) die_nomem();
  fd = open_read(fntemp);
  if (fd == -1) {
    x = error_str(errno);
    buffer_puts(&b,"unable to open ");
    buffer_puts(&b,fntemp);
    buffer_puts(&b,": ");
    buffer_puts(&b,x);
    return;
  }
  r = buffer_unixread(fd,status,sizeof status);
  close(fd);
  if (r < sizeof status) {
    if (r == -1)
      x = error_str(errno);
    else
      x = "bad format";
    buffer_puts(&b,"unable to read ");
    buffer_puts(&b,fntemp);
    buffer_puts(&b,": ");
    buffer_puts(&b,x);
    return;
  }

  pid = (unsigned char) status[15];
  pid <<= 8; pid += (unsigned char) status[14];
  pid <<= 8; pid += (unsigned char) status[13];
  pid <<= 8; pid += (unsigned char) status[12];

  paused = status[16];
  want = status[17];

  tai_unpack(status,&when);
  tai_now(&now);
  if (tai_less(&now,&when)) when = now;
  tai_sub(&when,&now,&when);

  if (pid) {
    buffer_puts(&b,"up (pid ");
    buffer_put(&b,strnum,fmt_ulong(strnum,pid));
    buffer_puts(&b,") ");
  }
  else
    buffer_puts(&b,"down ");

  buffer_put(&b,strnum,fmt_ulong(strnum,tai_approx(&when)));
  buffer_puts(&b," seconds");

  if (pid && !normallyup)
    buffer_puts(&b,", normally down");
  if (!pid && normallyup)
    buffer_puts(&b,", normally up");
  if (pid && paused)
    buffer_puts(&b,", paused");
  if (!pid && (want == 'u'))
    buffer_puts(&b,", want up");
  if (pid && (want == 'd'))
    buffer_puts(&b,", want down");
}

int main(int argc,char **argv)
{
  int fdorigdir;
  char *dir;

  ++argv;

  fdorigdir = open_read(".");
  if (fdorigdir == -1)
    strerr_die2sys(111,FATAL,"unable to open current directory: ");

  while (dir = *argv++) {
    doit(dir);
    buffer_puts(&b,"\n");
    if (fchdir(fdorigdir) == -1)
      strerr_die2sys(111,FATAL,"unable to set directory: ");
  }

  buffer_flush(&b);

  _exit(0);
}
