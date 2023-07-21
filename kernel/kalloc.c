// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

char ref_cnt[(PHYSTOP - KERNBASE) / PGSIZE];
struct spinlock ref_cnt_lock;
char ind = 1; // kinit() freerange

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

void
kinit()
{
  for (uint64 i = 0; i < sizeof(ref_cnt); i++)
    ref_cnt[i] = 0;

  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  ind = 0;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if (ind == 0 && ref_cnt_decrease_and_check((uint64) pa) == 1)
    return;

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
    ref_cnt_increase((uint64) r);
  }
  return (void*)r;
}

char
ref_cnt_decrease_and_check(uint64 pa)
{
  char cnt;

  cnt = ref_cnt_decrease(pa);
  
  return (cnt > 0) ? 1 : 0;
}

char
ref_cnt_increase(uint64 pa)
{
  char res;

  acquire(&ref_cnt_lock);
  res = ++ref_cnt[PA2REF(pa)];
  release(&ref_cnt_lock);

  return res;
}

char
ref_cnt_decrease(uint64 pa)
{
  char res;

  acquire(&ref_cnt_lock);
  if (ref_cnt[PA2REF(pa)] == 0)
    panic("ref_cnt_decrease");
  res = --ref_cnt[PA2REF(pa)];
  release(&ref_cnt_lock);

  return res;
}
