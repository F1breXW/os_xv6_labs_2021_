// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// 引用计数数组和锁
// 每个物理页面都有一个引用计数，表示有多少个页表引用该页面
#define REFCOUNT_SIZE ((PHYSTOP - KERNBASE) / PGSIZE)
int ref_count[REFCOUNT_SIZE];
struct spinlock ref_lock;

// 根据物理地址获取引用计数数组索引
int
pa2idx(uint64 pa)
{
  if(pa < KERNBASE || pa >= PHYSTOP)
    panic("pa2idx: invalid physical address");
  return (pa - KERNBASE) / PGSIZE;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref_lock, "ref_count");  // 初始化引用计数锁
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    // 初始化引用计数为1，然后调用kfree减少到0
    ref_count[pa2idx((uint64)p)] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // 减少引用计数，只有当引用计数为0时才真正释放页面
  acquire(&ref_lock);
  int idx = pa2idx((uint64)pa);
  if(idx < 0 || idx >= REFCOUNT_SIZE) {
    release(&ref_lock);
    panic("kfree: index out of bounds");
  }
  
  if(ref_count[idx] <= 0) {
    release(&ref_lock);
    panic("kfree: reference count underflow");
  }
  
  ref_count[idx]--;
  if(ref_count[idx] > 0) {
    // 还有其他引用，不释放
    release(&ref_lock);
    return;
  }
  release(&ref_lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // 设置新分配页面的引用计数为1
    acquire(&ref_lock);
    int idx = pa2idx((uint64)r);
    if(idx >= 0 && idx < REFCOUNT_SIZE) {
      ref_count[idx] = 1;
    } else {
      release(&ref_lock);
      panic("kalloc: index out of bounds");
    }
    release(&ref_lock);
  }
  return (void*)r;
}

// 增加页面引用计数
void
krefcnt_inc(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    panic("krefcnt_inc: invalid address");
  
  acquire(&ref_lock);
  int idx = pa2idx((uint64)pa);
  if(idx < 0 || idx >= REFCOUNT_SIZE)
    panic("krefcnt_inc: index out of bounds");
  ref_count[idx]++;
  release(&ref_lock);
}

// 获取页面引用计数
int
krefcnt_get(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    panic("krefcnt_get: invalid address");
  
  acquire(&ref_lock);
  int idx = pa2idx((uint64)pa);
  if(idx < 0 || idx >= REFCOUNT_SIZE)
    panic("krefcnt_get: index out of bounds");
  int count = ref_count[idx];
  release(&ref_lock);
  return count;
}
