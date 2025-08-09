//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "../include/types.h"
#include "../include/defs.h"
#include "../include/param.h"
#include "../include/stat.h"
#include "../include/mmu.h"
#include "../include/proc.h"
#include "../include/fs.h"
#include "../include/spinlock.h"
#include "../include/sleeplock.h"
#include "../include/file.h"
#include "../include/fcntl.h"
#include "../include/x86.h"

// sysfile.c

/*int sys_gtty(void) {
  struct file *f;
  struct ttyb u_ttyb;
  
  if (argfd(0, 0, &f) < 0 || argptr(1, (void*)&u_ttyb, sizeof(struct ttyb)) < 0)
    return -1;
  
  if (f->type != FD_INODE || !S_ISCHR(f->ip->type))
    return -1; // Not a character device

  struct tty *t = &ttys[f->ip->minor];
  acquire(&t->lock);
  
  // Copy TTY params to user
  u_ttyb.erase = t->erase;
  u_ttyb.kill = t->kill;
  u_ttyb.tflags = t->tflags;
  
  release(&t->lock);
  return 0;
}*/

//int sys_stty(void) {
//  // ... (similar to gtty, but copy from user to TTY)
//}

struct vga_meta {
    int x, y;
    int width, height;
};

int
sys_devctl(void)
{
  int dev;
  int sig;
  int data; // only used on writes

  if(argint(0, &dev) < 0 || argint(1, &sig) < 0 || argint(2, &data) < 0 )
    return -1;

  if (dev == 0){ // Mouse
	  if (sig == 0){
		int packed = ((g_mouse_x_pos & 0xFFF) << 12) |
			(g_mouse_y_pos & 0xFFF);
		return packed;
	  }else if (sig == 1){
		return -1;  //dont support writes to mousedev
	  }else if (sig == 2){
		int packed = (left_button << 8) | (right_button << 4) | middle_button;
		return packed;
	  }
  } else
  if (dev == 1){ // vga/screen
	if (sig == 0){
		int packed = ((VGA_MAX_WIDTH) << 12) | (VGA_MAX_HEIGHT & 0xFFF);
		return packed;
	}else
	if (sig == 1) { // write pixel
		int x = (data >> 22) & 0x3FF;   // 10 bits for X
		int y = (data >> 12) & 0x3FF;   // 10 bits for Y
		int color = data & 0xFF;         // 8 bits for color (use lower 4 bits)

		if (x < VGA_MAX_WIDTH && y < VGA_MAX_HEIGHT) {
			putpixel(x, y, color & 0x0F);  // Use only lower 4 bits for 16 colors
			return 0;
		} else
			return -1;
	}else
	if (sig == 2) {
		vga_clear_screen(data);
		return 0;
	}
	else if (sig == 3) {  // Bulk update
    		char* user_ptr;
    		if (argptr(2, (void*)&user_ptr, sizeof(struct vga_meta)) < 0)
        		return -1;

    		struct vga_meta* meta = (struct vga_meta*)user_ptr;
    
    		if (meta->width <= 0 || meta->height <= 0 ||
        		meta->x < 0 || meta->x + meta->width > VGA_MAX_WIDTH ||
        		meta->y < 0 || meta->y + meta->height > VGA_MAX_HEIGHT) {
       			return -1;
    		}

    		size_t buf_size = meta->width * meta->height;
    		if (argptr(2, (void*)&user_ptr, sizeof(struct vga_meta) + buf_size) < 0)
        		return -1;

    		uint8_t* pixels = (uint8_t*)(user_ptr + sizeof(struct vga_meta));
    		for (int rel_y = 0; rel_y < meta->height; rel_y++) {
        		for (int rel_x = 0; rel_x < meta->width; rel_x++) {
            			int abs_x = meta->x + rel_x;
            			int abs_y = meta->y + rel_y;
            			uint8_t color = pixels[rel_y * meta->width + rel_x];
            			putpixel(abs_x, abs_y, color & 0x0F);
        		}
    		}
    		return 0;
	}
	if (sig == 4) { // Bulk read
		char* user_ptr;
		if (argptr(2, (void*)&user_ptr, sizeof(struct vga_meta)) < 0)
			return -1;

		struct vga_meta* meta = (struct vga_meta*)user_ptr;
		size_t buf_size = meta->width * meta->height;

		if (meta->width <= 0 || meta->height <= 0 || buf_size > 1024 ||
			meta->x < 0 || meta->x + meta->width > VGA_MAX_WIDTH ||
        		meta->y < 0 || meta->y + meta->height > VGA_MAX_HEIGHT) {
        		return -1;
    		}

    		if (argptr(2, (void*)&user_ptr, sizeof(struct vga_meta) + buf_size) < 0)
        		return -1;

    		uint8_t* pixels = (uint8_t*)(user_ptr + sizeof(struct vga_meta));
    		uint8_t temp_buf[1024]; // Temporary kernel buffer

    		for (int rel_y = 0; rel_y < meta->height; rel_y++) {
        		for (int rel_x = 0; rel_x < meta->width; rel_x++) {
            			int abs_x = meta->x + rel_x;
            			int abs_y = meta->y + rel_y;
            			temp_buf[rel_y * meta->width + rel_x] = getpixel(abs_x, abs_y);
        		}
    		}

    		if (copyout(myproc()->pgdir, (uint)pixels, temp_buf, buf_size) < 0)
        		return -1;

    		return 0;
	}

  }
  return -1;
}

/*
int
sys_devctl(void)
{
  int dev;
  int sig;
  int buf;

  if(argint(0, &dev) < 0 || argint(1, &sig) < 0 || argint(2, &buf) < 0)
    return -1;
  switch (dev) {
	case 0: // mouse
		if (sig) { // read
			int pos = (g_mouse_x_pos << 16) | (g_mouse_y_pos & 0xFFFF);
			buf = pos;
			cprintf("%x", pos);
		} else
			return -1; // bad call
		break;
  }
  return 0;
}*/

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int
sys_sync(void)
{
  sync();
  return 0;
}

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

int
sys_lseek(void)
{
    struct file *f;
    int offset;
    int whence;

    if (argfd(0, 0, &f) < 0 ||
        argint(1, &offset) < 0 ||
        argint(2, &whence) < 0)
        return -1;

    if (f->type == FD_PIPE)
        return -1;

    ilock(f->ip);

    switch (whence) {
    case SEEK_SET:
        f->off = offset;
        break;
    case SEEK_CUR:
        f->off += offset;
        break;
    case SEEK_END:
        f->off = f->ip->size + offset;
        break;
    default:
        iunlock(f->ip);
        return -1;
    }

    iunlock(f->ip);
    return f->off;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int sys_fstat(void) {
  struct file *f;
  struct stat st;
  struct stat *user_st;

  if(argfd(0, 0, &f) < 0 ||
     argptr(1, (void*)&user_st, sizeof(struct stat)) < 0)
    return -1;

  // Use memset to ensure padding is zeroed
  memset(&st, 0, sizeof(st));

  stati(f->ip, &st);

  // Copy entire struct including padding
  if(copyout(myproc()->pgdir, (uint)user_st, &st, sizeof(st)) < 0)
    return -1;

  return 0;
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->mode == S_IFDIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->mode == S_IFDIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->mode == S_IFDIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{

  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0) {
    return 0;
  }
  ilock(dp);

  if ((ip = dirlookup(dp, name, 0)) != 0) {
    iunlockput(dp);
    ilock(ip);
    if ((type & S_IFMT) == S_IFREG && (ip->mode & S_IFMT) == S_IFREG || (ip->mode & S_IFMT) == S_IFCHR) {
        return ip;
    }
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->mode = type;
  ip->nlink = 1;
  ip->ctime = epoch_mktime();
  ip->lmtime = epoch_mktime();
  iupdate(ip);

  if(type == S_IFDIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, S_IFREG, 0, 0);
    ip->lmtime = epoch_mktime();
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);

    if(ip->mode == S_IFDIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f){
	    ip->lmtime = epoch_mktime();
      fileclose(f);
    }
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();
//  ip->lmtime = epoch_mktime();
  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;

  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, S_IFDIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, S_IFCHR, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->mode != S_IFDIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}
