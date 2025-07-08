#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 获取路径中的文件名部分
char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;
  // 找到最后一个'/'后的第一个字符
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  // 拷贝文件名到buf
  memset(buf, 0, sizeof(buf));
  memmove(buf, p, strlen(p));
  return buf;
}

// 递归查找函数
void find(char *path, char *target) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  // 打开目录或文件
  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }
  // 获取文件状态
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  // 判断类型
  switch(st.type){
  case T_FILE:
    // 如果是文件且文件名与目标名相同，则输出路径
    if(strcmp(fmtname(path), target) == 0)
      printf("%s\n", path);
    break;
  case T_DIR:
    // 拼接路径，递归进入子目录
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      // 跳过 "." 和 ".."
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      // 获取子文件/目录的状态
      if(stat(buf, &st) < 0){
        printf("find: cannot stat %s\n", buf);
        continue;
      }
      // 递归查找
      find(buf, target);
    }
    break;
  }
  close(fd);
}

int 
main(int argc, char *argv[]){
  // 参数检查，必须为2个参数：路径和目标文件名
  if(argc != 3){
    fprintf(2, "usage: find <path> <filename>\n");
    exit(1);
  }
  // 从指定路径递归查找目标文件名
  find(argv[1], argv[2]);
  exit(0);
}
