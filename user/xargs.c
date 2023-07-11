#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"


#define MAX_ARG_SIZE 64

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(2, "Usage: xargs executable [options]\n");
    exit(1);
  }


  char *args[MAXARG];
  int args_cnt = 0;
  for (int i = 1; i < argc; i++)
    args[args_cnt++] = argv[i];
  
  
  char buf;
  char *curr = (char *) malloc(MAX_ARG_SIZE);
  int n, cnt = 0;

  while ((n = read(0, &buf, 1)) > 0) {
    if (buf == ' ' || buf == '\n') {
      if (cnt == 0)
        continue;
      curr[cnt] = '\0';
      args[args_cnt++] = curr;
      curr = (char *) malloc(MAX_ARG_SIZE);
      cnt = 0;

      if (buf == '\n') {
        args[args_cnt] = 0;

        int pid = fork();
        if (pid < 0) {
          fprintf(2, "xargs: fork failed\n");
          exit(1);
        }
        
        if (pid == 0) {
          exec(args[0], args);
          fprintf(2, "xargs: exec failed\n");
          exit(1);
        }

        wait((int *) 0);
        for (int i = argc - 1; i < args_cnt; i++) 
          free(args[i]);

        args_cnt = argc - 1;
      }
    }
    else {
      curr[cnt++] = buf;
    }
  }
  if (n < 0) {
    fprintf(2, "xargs: read\n");
    exit(1);
  }

  
  free(curr);

  exit(0);
}
