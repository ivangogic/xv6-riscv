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


#define LOCK_NAME_LEN  8
#define NBUCKETS 13
#define HASH(dev, blockno) (((((blockno) & 0xFFFF) << 16) | ((dev) & 0xFFFF)) % NBUCKETS);

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  struct buf buckets[NBUCKETS];
  struct spinlock bucket_locks[NBUCKETS];
  char lock_names[NBUCKETS][LOCK_NAME_LEN];
} bcache;

void
binit(void)
{
  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NBUCKETS; i++) {
    snprintf(bcache.lock_names[i], LOCK_NAME_LEN, "bcache%d", i);
    initlock(&bcache.bucket_locks[i], bcache.lock_names[i]);
    bcache.buckets[i].next = 0;
  }

  for (int i = 0; i < NBUF; i++) {
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->refcnt = 0;
    b->usage = 0;
    b->next = bcache.buckets[i % NBUCKETS].next;
    bcache.buckets[i % NBUCKETS].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint hash = HASH(dev, blockno);
  acquire(&bcache.bucket_locks[hash]);

  // Is the block already cached?
  for(b = bcache.buckets[hash].next; b != 0; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_locks[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  release(&bcache.bucket_locks[hash]);
  acquire(&bcache.lock);

  // Check if the block is already cached (again)
  for(b = bcache.buckets[hash].next; b != 0; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      acquire(&bcache.bucket_locks[hash]);
      b->refcnt++;
      release(&bcache.bucket_locks[hash]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  struct buf *to_remove_before = 0;
  int lock_index = -1, lru = __INT_MAX__;
  for(int i = 0; i < NBUCKETS; i++) {
    acquire(&bcache.bucket_locks[i]);
    int here = 0;
    for(b = &bcache.buckets[i]; b->next != 0; b = b->next) {
      if (b->next->refcnt == 0 && b->next->usage < lru) {
        lru = b->next->usage;
        to_remove_before = b;
        here = 1;
      }
    }
    if (here) {
      if (lock_index != -1)
        release(&bcache.bucket_locks[lock_index]);
      lock_index = i;
    }
    else
      release(&bcache.bucket_locks[i]);
  }
  
  if (to_remove_before) {
    if (lock_index != hash)
      acquire(&bcache.bucket_locks[hash]);
    b = to_remove_before->next;
    to_remove_before->next = b->next;
    b->next = bcache.buckets[hash].next;
    bcache.buckets[hash].next = b;
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.bucket_locks[hash]);
    if (lock_index != hash)
      release(&bcache.bucket_locks[lock_index]);
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }

  panic("bget: no buffers");
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
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint hash = HASH(b->dev, b->blockno);

  acquire(&bcache.bucket_locks[hash]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->usage = ticks;
  }
  
  release(&bcache.bucket_locks[hash]);
}

void
bpin(struct buf *b) {
  uint hash = HASH(b->dev, b->blockno);
  acquire(&bcache.bucket_locks[hash]);
  b->refcnt++;
  release(&bcache.bucket_locks[hash]);
}

void
bunpin(struct buf *b) {
  uint hash = HASH(b->dev, b->blockno);
  acquire(&bcache.bucket_locks[hash]);
  b->refcnt--;
  release(&bcache.bucket_locks[hash]);
}
