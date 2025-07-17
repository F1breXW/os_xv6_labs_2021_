/*
 * pgaccess系统调用测试程序
 * 用于测试页面访问位检测功能
 */
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/riscv.h"      // 包含PGSIZE定义
#include "kernel/memlayout.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char *buf;
  unsigned int abits;
  
  printf("pgaccess test starting\n");
  
  // 测试基本功能 - 分配4个页面
  buf = malloc(4 * PGSIZE);
  if (buf == 0) {
    printf("malloc failed\n");
    exit(1);
  }
  
  // 访问第0页和第2页，触发访问位设置
  buf[PGSIZE * 0] = 1;
  buf[PGSIZE * 2] = 1;
  
  // 检查哪些页面被访问过
  if (pgaccess(buf, 4, &abits) < 0) {
    printf("pgaccess failed\n");
    exit(1);
  }
  
  printf("access bits: %x\n", abits);
  
  // 应该设置第0位和第2位 (0x5 = 0101)
  if ((abits & 0x5) != 0x5) {
    printf("pgaccess: expected bits 0 and 2 to be set, got %x\n", abits);
    exit(1);
  }
  
  free(buf);
  printf("pgaccess test: OK\n");
  exit(0);
}
