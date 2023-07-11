#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"


void 
primes(int pajp[]) 
{  
  int tmp_p[2];
  char buf;
  int p = -1;

  int n = read(pajp[0], &buf, 1);

  if (n <= 0) {
    close(pajp[0]);
    return;
  }

  p = buf;
  printf("prime %d\n", p);
  pipe(tmp_p);
  int pid = fork();

  if (pid < 0) {
    fprintf(2, "primes: fork\n");
    exit(1);
  }

  if (pid == 0) {
    close(tmp_p[1]);
    primes(tmp_p);
  }
  else {
    close(tmp_p[0]);

    while (read(pajp[0], &buf, 1) > 0)
      if (buf % p != 0)
        write(tmp_p[1], &buf, 1);
      

    close(tmp_p[1]);
    wait((int *) 0);
  }
}

int
main(int argc, char *argv[])
{
  int pajp[2];

  pipe(pajp);

  int pid = fork();

  if (pid < 0) {
    fprintf(2, "primes: fork\n");
    exit(1);
  }
  else if (pid == 0) {
    close(pajp[1]);
    primes(pajp);
  }
  else {
    close(pajp[0]);

    for (char i = 2; i <= 35; i++)
      write(pajp[1], &i, 1);

    close(pajp[1]);
    wait((int *) 0);
  }


  exit(0);
}
