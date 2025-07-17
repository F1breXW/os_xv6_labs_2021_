#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
// pgaccess系统调用 - 检测页面访问位
// 参数: addr(起始虚拟地址), len(页面数量), mask_addr(结果掩码地址)
// 返回: 成功返回0，失败返回-1
int
sys_pgaccess(void)
{
  uint64 addr;
  int len;
  uint64 mask_addr;

  // 获取用户传入的参数
  if (argaddr(0, &addr) < 0 || argint(1, &len) < 0 || argaddr(2, &mask_addr) < 0)
    return -1;

  // 限制检查的页面数量（防止过度计算）
  if (len > 64) len = 64;

  struct proc *p = myproc();
  uint64 mask = 0;

  // 遍历指定范围的页面，检查访问位
  for (int i = 0; i < len; i++) {
    pte_t *pte = walk(p->pagetable, addr + i * PGSIZE, 0);
    if (pte && (*pte & PTE_V) && (*pte & PTE_A)) {
      mask |= (1L << i);        // 在掩码中设置相应位
      *pte &= ~PTE_A;          // 清除访问位，为下次检测做准备
    }
  }

  // 将结果掩码复制到用户空间
  if (copyout(p->pagetable, mask_addr, (char *)&mask, sizeof(mask)) < 0)
    return -1;

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
