#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
  int p2c[2], c2p[2];
  char buf[1];

  pipe(p2c);
  pipe(c2p);

  if (fork() == 0) {
    close(p2c[1]);
    close(c2p[0]);

    read(p2c[0], buf, 1);
    close(p2c[0]);
    printf("%d: received ping\n", getpid());

    write(c2p[1], "p", 1);
    close(c2p[1]);
  }
  else {
    close(p2c[0]);
    close(c2p[1]);

    write(p2c[1], "p", 1);
    close(p2c[1]);

    read(c2p[0], buf, 1);
    close(c2p[0]);
    printf("%d: received pong\n", getpid());
  }

  exit(0);
}
