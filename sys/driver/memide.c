// Fake IDE disk; stores blocks in memory.
// Useful for running kernel without scratch disk.

#include <types.h>
#include <defs.h>
#include <param.h>
#include <memlayout.h>
#include <mmu.h>
#include <proc.h>
#include <x86.h>
#include <traps.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <fs.h>
#include <buf.h>
#include <multiboot.h>

static uint disksize;
static uchar *memdisk;

void
ideinit(void)
{
  struct multiboot_tag_module * mod = modget(MULTIBOOT_TAG_TYPE_MODULE);
  if (mod == 0) {
    panic("memdisk not loaded");
  }
  cprintf("memdisk range = 0x%08x - 0x%08x\n", P2V(mod->mod_start), P2V(mod->mod_end - mod->mod_start));
  memdisk = P2V(mod->mod_start);
  disksize = (uint)(mod->mod_end - mod->mod_start)/BSIZE;
}

// Interrupt handler.
void
ideintr(void)
{
  // no-op
}

// Sync buf with disk.
// If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
// Else if B_VALID is not set, read buf from disk, set B_VALID.
void
iderw(struct buf *b)
{
  uchar *p;

  if(!holdingsleep(&b->lock))
    panic("iderw: buf not locked");
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
    panic("iderw: nothing to do");
  if(b->dev != 1)
    panic("iderw: request not for disk 1");
  if(b->blockno >= disksize)
    panic("iderw: block out of range");

  p = memdisk + b->blockno*BSIZE;

  if(b->flags & B_DIRTY){
    b->flags &= ~B_DIRTY;
    memmove(p, b->data, BSIZE);
  } else
    memmove(b->data, p, BSIZE);
  b->flags |= B_VALID;
}
