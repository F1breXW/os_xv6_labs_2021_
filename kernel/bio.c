// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct hashbucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct hashbucket bucket[NBUCKET];
} bcache;

int
hash(uint blockno)
{
  return blockno % NBUCKET;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Initialize hash buckets
  for(int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.bucket[i].lock, "bcache.bucket");
    bcache.bucket[i].head.prev = &bcache.bucket[i].head;
    bcache.bucket[i].head.next = &bcache.bucket[i].head;
  }

  // Initialize all buffers and put them in bucket 0
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.bucket[0].head.next;
    b->prev = &bcache.bucket[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.bucket[0].head.next->prev = b;
    bcache.bucket[0].head.next = b;
    b->timestamp = 0;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucket_id = hash(blockno);

  acquire(&bcache.bucket[bucket_id].lock);

  // Is the block already cached?
  for(b = bcache.bucket[bucket_id].head.next; b != &bcache.bucket[bucket_id].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);
      release(&bcache.bucket[bucket_id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached; recycle an unused buffer in this bucket.
  for(b = bcache.bucket[bucket_id].head.prev; b != &bcache.bucket[bucket_id].head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);
      release(&bcache.bucket[bucket_id].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.bucket[bucket_id].lock);
  
  // Serialize eviction with global lock
  acquire(&bcache.lock);
  
  // Check target bucket again
  acquire(&bcache.bucket[bucket_id].lock);
  for(b = bcache.bucket[bucket_id].head.next; b != &bcache.bucket[bucket_id].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);
      release(&bcache.bucket[bucket_id].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  for(b = bcache.bucket[bucket_id].head.prev; b != &bcache.bucket[bucket_id].head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);
      release(&bcache.bucket[bucket_id].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket[bucket_id].lock);

  // Search other buckets for LRU
  struct buf *lru = 0;
  int lru_bucket = -1;
  for(int i = 0; i < NBUCKET; i++) {
    if(i == bucket_id) continue;
    
    acquire(&bcache.bucket[i].lock);
    for(b = bcache.bucket[i].head.prev; b != &bcache.bucket[i].head; b = b->prev){
      if(b->refcnt == 0 && (lru == 0 || b->timestamp < lru->timestamp)) {
        lru = b;
        lru_bucket = i;
      }
    }
    release(&bcache.bucket[i].lock);
  }

  if(lru == 0) {
    release(&bcache.lock);
    panic("bget: no buffers");
  }

  // Move LRU buffer to target bucket
  acquire(&bcache.bucket[lru_bucket].lock);
  if(lru->refcnt != 0) {
    // Someone took it, try again
    release(&bcache.bucket[lru_bucket].lock);
    release(&bcache.lock);
    return bget(dev, blockno);
  }
  
  // Remove from old bucket
  lru->next->prev = lru->prev;
  lru->prev->next = lru->next;
  release(&bcache.bucket[lru_bucket].lock);

  // Add to new bucket
  acquire(&bcache.bucket[bucket_id].lock);
  lru->next = bcache.bucket[bucket_id].head.next;
  lru->prev = &bcache.bucket[bucket_id].head;
  bcache.bucket[bucket_id].head.next->prev = lru;
  bcache.bucket[bucket_id].head.next = lru;
  
  lru->dev = dev;
  lru->blockno = blockno;
  lru->valid = 0;
  lru->refcnt = 1;
  acquire(&tickslock);
  lru->timestamp = ticks;
  release(&tickslock);
  
  release(&bcache.bucket[bucket_id].lock);
  release(&bcache.lock);
  acquiresleep(&lru->lock);
  return lru;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list when refcnt becomes 0.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket_id = hash(b->blockno);
  acquire(&bcache.bucket[bucket_id].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // Move to head of LRU list for this bucket
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bucket[bucket_id].head.next;
    b->prev = &bcache.bucket[bucket_id].head;
    bcache.bucket[bucket_id].head.next->prev = b;
    bcache.bucket[bucket_id].head.next = b;
  }
  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);
  release(&bcache.bucket[bucket_id].lock);
}

void
bpin(struct buf *b) {
  int bucket_id = hash(b->blockno);
  acquire(&bcache.bucket[bucket_id].lock);
  b->refcnt++;
  release(&bcache.bucket[bucket_id].lock);
}

void
bunpin(struct buf *b) {
  int bucket_id = hash(b->blockno);
  acquire(&bcache.bucket[bucket_id].lock);
  b->refcnt--;
  release(&bcache.bucket[bucket_id].lock);
}


