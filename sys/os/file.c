//
// File descriptors
//

#include <types.h>
#include <defs.h>
#include <param.h>
#include <fs.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <file.h>
#include <mmu.h>
#include <proc.h>
#include <stat.h>

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

int badwrite(struct inode *ip, char *buf, int n) { return -1; }
int badread(struct inode *ip, char *buf, int n) { return -1; }
int nullread(struct inode *ip, char *buf, int n) { return 0; }

// Devnodes devices
uint32_t
xorshift32(uint32_t *state)
{
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

static uint32_t rng_state;

// very simple entropy
int
rndread(struct inode *ip, char *dst, int n)
{
  uint32_t lo, hi;

  // Generate entropy using rdtsc and mix into PRNG state
  asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
  uint32_t entropy = lo ^ hi;
  rng_state ^= entropy; // Mix entropy into existing state

  uint32_t *buf32 = (uint32_t*)dst;
  int num_words = n / sizeof(uint32_t);
  int remainder = n % sizeof(uint32_t);

  for (int i = 0; i < num_words; i++) {
      buf32[i] = xorshift32(&rng_state);
  }

  if (remainder > 0) {
      uint32_t val = xorshift32(&rng_state);
      char *tail = (char*)(buf32 + num_words);
      for (int i = 0; i < remainder; i++) {
          tail[i] = (val >> (i * 8)) & 0xFF;
      }
  }

  return n;
}

/*int
scrwrite(struct inode *ip, char *src, int n)
{
    int bytes_processed = 0;
    while (n - bytes_processed > 0) {
        uint8_t type = (uint8_t)src[bytes_processed];
        bytes_processed++;

        switch (type) {
            case 0x00: {  // PutPixel
                if (n - bytes_processed < 5) return n;
                uint16_t x = (uint8_t)src[bytes_processed] | ((uint8_t)src[bytes_processed+1] << 8);
                uint16_t y = (uint8_t)src[bytes_processed+2] | ((uint8_t)src[bytes_processed+3] << 8);
                uint8_t c = (uint8_t)src[bytes_processed+4];
                putpixel(x, y, c);
                bytes_processed += 5;
                break;
            }

            case 0x01:   // Line
            case 0x02:   // Rectangle
            case 0x03:   // Filled Rectangle
            case 0x04:   // Circle
	    case 0x05: { // Clear
                if (n - bytes_processed < 9) return n;
                uint16_t x1 = (uint8_t)src[bytes_processed] | ((uint8_t)src[bytes_processed+1] << 8);
                uint16_t y1 = (uint8_t)src[bytes_processed+2] | ((uint8_t)src[bytes_processed+3] << 8);
                uint16_t x2 = (uint8_t)src[bytes_processed+4] | ((uint8_t)src[bytes_processed+5] << 8);
                uint16_t y2 = (uint8_t)src[bytes_processed+6] | ((uint8_t)src[bytes_processed+7] << 8);
                uint8_t c = (uint8_t)src[bytes_processed+8];

                switch (type) {
                    case 0x01: putline(x1, y1, x2, y2, c); break;
                    case 0x02: putrect(x1, y1, x2, y2, c); break;
                    case 0x03: putrectf(x1, y1, x2, y2, c); break;
                    case 0x04: putcircle(x1, y1, x2, c); break;
		    case 0x05: vga_clear_screen(0); break;
                }
                bytes_processed += 9;
                break;
            }
            default:  // Unknown command
                return n;
        }
    }
    return n;
}*/

int
keyread(struct inode *ip, char *dst, int n)
{
  return n;
}

int mouseread(struct inode *ip, char *dst, int n) {
    return n;
}

// Devnodes devices (not /dev/console)
void
devinit()
{
  devsw[NULLDEV].read = nullread;
  devsw[NULLDEV].write = badwrite;
  devsw[RANDOM].read = rndread;
  devsw[RANDOM].write = badwrite;
  devsw[SCRDEV].read = badread;
//  devsw[SCRDEV].write = scrwrite;
  devsw[KEYDEV].read = keyread;
  devsw[KEYDEV].write = badwrite;
  devsw[MOUSDEV].read = mouseread;
  devsw[MOUSDEV].write = badwrite;
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  if(f->readable == 0){
    errno = 13;
    return -1;
  }
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  if(f->writable == 0) {
    errno = 13;
    return -1;
  }
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if (myproc()->p_uid != f->ip->uid &&
          ((f->ip->mode & S_IFMT) != S_IFCHR) &&
          ((f->ip->mode & S_IFMT) != S_IFBLK)) {
        iunlock(f->ip);
        end_op();
	errno = 1;
        return -1;
      }
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0){
        f->off += r;
        f->ip->lmtime = epoch_mktime();
        iupdate(f->ip);
      }
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}

