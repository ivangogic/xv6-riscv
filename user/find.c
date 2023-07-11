#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


void
find(char* dir_path, char* name) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(dir_path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", dir_path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", dir_path);
    close(fd);
    return;
  }

  if (st.type != T_DIR) {
    fprintf(2, "find: provided name not directory\n");
  }

  if (strlen(dir_path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
    printf("find: path too long\n");
    return;
  }
  strcpy(buf, dir_path);
  p = buf + strlen(buf);
  *p++ = '/';
  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0)
      continue;
    memmove(p, de.name, DIRSIZ);
    p[DIRSIZ] = 0;
    if (stat(buf, &st) < 0)  {
      printf("find: cannot stat %s\n", buf);
      continue;
    }

    switch (st.type) {
      case T_FILE:
        if (strcmp(name, p) == 0)
          printf("%s\n", buf);
        break;
      case T_DIR:
        if (!(strcmp(p, ".") == 0) && !(strcmp(p, "..") == 0)) {
          find(buf, name);
        }
        break;
      default:
       break;
    }
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc != 3) {
    fprintf(2, "Usage: find dir file_name\n");
    exit(1);
  }

  find(argv[1], argv[2]);

  exit(0);
}
