/*
 * sys.c - i386 system calls. System calls are in the order defined in syscall.h.
 */

#include <types.h>
#include <defs.h>
#include <param.h>
#include <stat.h>
#include <mmu.h>
#include <proc.h>
#include <fs.h>
#include <spinlock.h>
#include <sleeplock.h>
#include <file.h>
#include <fcntl.h>
#include <x86.h>
#include <tty.h>
#include <utsname.h>
#include <version.h>
#include <errno.h>
#include <time.h>
#include <ioctls.h>

extern struct ttyb ttyb;
extern struct cons cons;
char sys_nodename[65] = "localhost";
char sys_domainname[65] = "domainname";

/*
 * this indicates wether you can reboot with ctrl-alt-del: the default is yes
 */
static int C_A_D = 1;

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int fdalloc(struct file *f){
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

void notim(){
	printk("syscall %d: Not Implemented.\n", myproc()->tf->eax);
}

/* both setre* functions are from linux 0.11 */
int setreuid(int ruid, int euid){
	int old_ruid = myproc()->uid;
	struct proc *p = myproc();

	if (ruid>0) {
		if ((p->euid==ruid) ||
                    (old_ruid == ruid) ||
		    suser())
			p->uid = ruid;
		else
			return -EPERM;
	}
	if (euid>0) {
		if ((old_ruid == euid) ||
                    (p->euid == euid) ||
		    suser())
			p->euid = euid;
		else {
			p->uid = old_ruid;
			return -EPERM;
		}
	}
	return 0;
}

int setregid(int rgid, int egid){
	struct proc *p = myproc();

	if (rgid>0) {
		if ((p->gid == rgid) || 
		    suser())
			p->gid = rgid;
		else
			return -EPERM;
	}
	if (egid>0) {
		if ((p->gid == egid) ||
		    (p->egid == egid) ||
		    (p->sgid == egid) ||
		    suser())
			p->egid = egid;
		else
			return -EPERM;
	}
	return 0;
}

// Is the directory dp empty except for "." and ".." ?
static int isdirempty(struct inode *dp){
	int off;
	struct dirent de;

	for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
		if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
			panic("isdirempty: readi");
		if(de.d_ino != 0)
			return 0;
	}
	return 1;
}

static int argfd(int n, int *pfd, struct file **pf){
	int fd;
	struct file *f;

	if(argint(n, &fd) < 0)
		return -EINVAL;

	if(fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0)
		return -ENOENT;

	if(pfd)
		*pfd = fd;
	if(pf)
		*pf = f;
	return 0;
}

static struct inode* create(char *path, short type, short major, short minor){

	struct inode *ip, *dp;
	char name[DIRSIZ];

	if((dp = nameiparent(path, name)) == 0)
		return 0;

	ilock(dp);

	if ((ip = dirlookup(dp, name, 0)) != 0) {
		iunlockput(dp);
		ilock(ip);
		if ((type & S_IFMT) == S_IFREG) {
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
	ip->uid = myproc()->uid;
	ip->gid = myproc()->gid;
	iupdate(ip);

	if((ip->mode & S_IFMT) == S_IFDIR){	// Create . and .. entries.
		dp->nlink++;	// for ".."
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

/*
 * (This routine is effectively copied from Linux circa 1.0. Most
 * things related to rebooting and ctrl+alt+del are!)
 *
 * This routine reboots the machine by asking the keyboard
 * controller to pulse the reset-line low. We try that for a while,
 * and if it doesn't work, we do some other stupid things.
 */

long no_idt[2] = {0, 0};

void hard_reset_now(void){
	int i, j;

	sti();
	for (;;) {
		for (i=0; i<100; i++) {
			for(j = 0; j < 100000 ; j++)
				/* nothing */;
			outb(0x64,0xfe);	 /* pulse reset low */
		}
		__asm__("\tlidt no_idt");
	}
}

/*
 * This function gets called by ctrl-alt-del - ie the keyboard interrupt.
 * As it's called within an interrupt, it may NOT sync: the only choice
 * is wether to reboot at once, or just ignore the ctrl-alt-del.
 */
void ctrl_alt_del(void){
	if (C_A_D) {
		hard_reset_now();
	} else {
		myproc()->sigpending = SIGINT;
	}
}

/*
 * actual syscalls start here!
 */

/*
 * Placeholder. Nothing should use syscall 0
 */
int sys_syscall(void){
	notim();
	return -ENOSYS;
}

void sys_exit(void){
	int status;
	if(argint(0, &status) < 0)
		return;

	exit(status);
}

int sys_fork(void){
	return fork();
}

int sys_read(void){
	struct file *f;
	int n;
	char *p;

	if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
		return -EINVAL;

	return fileread(f, p, n);
}

int sys_write(void){
	struct file *f;
	int n;
	char *p;

	if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
		return -EINVAL;

	return filewrite(f, p, n);
}

int sys_open(void){
	char *path;
	int fd, omode;
	int mode = 0;
	struct file *f;
	struct inode *ip;

	if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
		return -EINVAL;

	if (omode & O_CREAT)
		argint(2, &mode);

	begin_op();

	if(omode & O_CREAT){
		ip = create(path, (S_IFREG | mode) & ~myproc()->umask, 0, 0);
		if(ip == 0){
			end_op();
			return -ENOENT;
		}
	} else {
		if((ip = namei(path)) == 0){
			end_op();
			return -ENOENT;
		}
		ilock(ip);
	}
	if (omode & O_TRUNC)
		itrunc(ip);

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
	if (omode & O_APPEND) {
		f->off = ip->size;
	} else {
		f->off = 0;
	}
	end_op();
	f->type = FD_INODE;
	f->ip = ip;
	f->readable = !(omode & O_WRONLY);
	f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
	return fd;
}

int sys_close(void){
	int fd;
	struct file *f;

	if(argfd(0, &fd, &f) < 0)
		return -EINVAL;

	myproc()->ofile[fd] = 0;
	fileclose(f);
	return 0;
}

int sys_waitpid(void){
	int *status;
	if(argptr(0, (void*)&status, sizeof(*status)) < 0)
		return -EINVAL;

	return (waitpid(-1, status, 0)); // Wait for any child
}

int sys_creat(void){
	char *path;
	int mode;
	int fd;
	struct inode *ip;
	struct file *f;

	if(argstr(0,&path)<0 || argint(1,&mode)<0)
		return -EINVAL;

	begin_op();

	/* forced create+truncate */
	ip=create(path,(S_IFREG|mode) & ~myproc()->umask,0,0);
	if(ip==0){
		end_op();
		return -ENOENT;
	}

	itrunc(ip);

	if((f=filealloc())==0 || (fd=fdalloc(f))<0){
		if(f){
			ip->lmtime=epoch_mktime();
			fileclose(f);
		}
		iunlockput(ip);
		end_op();
		return -ENOENT;
	}

	iunlock(ip);
	end_op();

	f->type=FD_INODE;
	f->ip=ip;
	f->readable=0;
	f->writable=1;
	f->off=0;

	return fd;
}

int sys_link(void){
	char name[DIRSIZ], *new, *old;
	struct inode *dp, *ip;

	if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
		return -EINVAL;

	begin_op();
	if((ip = namei(old)) == 0){
		end_op();
		return -ENOENT;
	}

	ilock(ip);

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
	return -ENOENT;
}

int sys_unlink(void){
	struct inode *ip, *dp;
	struct dirent de;
	char name[DIRSIZ], *path;
	unsigned int off;

	if(argstr(0, &path) < 0)
		return -EINVAL;

	begin_op();
	if((dp = nameiparent(path, name)) == 0){
		end_op();
		return -ENOENT;
	}

	ilock(dp);

	if (myproc()->uid != dp->uid && ((dp->mode & S_IFMT) != S_IFCHR) && ((dp->mode & S_IFMT) != S_IFBLK)) {
		iunlock(dp);
		end_op();
		return -EACCES;
	}

	// Cannot unlink "." or "..".
	if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
		goto bad;

	if((ip = dirlookup(dp, name, &off)) == 0)
		goto bad;

	ilock(ip);

	if(ip->nlink < 1)
		panic("unlink: nlink < 1");
	if(((ip->mode & S_IFMT) == S_IFDIR) && !isdirempty(ip)) {
		iunlockput(ip);
		goto bad;
	}

	memset(&de, 0, sizeof(de));
	if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
		panic("unlink: writei");
	if((ip->mode & S_IFMT) == S_IFDIR){
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
	return -ENOENT;
}

int sys_exec(void){
	char *path, *argv[MAXARG];
	int i;
	unsigned int uargv, uarg;

	if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0)
		return -EINVAL;

	memset(argv, 0, sizeof(argv));
	for(i=0;; i++){
		if(i >= NELEM(argv))
			return -EINVAL;
		if(fetchint(uargv+4*i, (int*)&uarg) < 0)
			return -ENOMEM;
		if(uarg == 0){
			argv[i] = 0;
			break;
		}
		if(fetchstr(uarg, &argv[i]) < 0)
			return -EPERM;
	}
	struct inode *ip;
	begin_op();
	ip = namei(path);
	if(ip == 0){
		end_op();
		return -ENOENT;
	}

	ilock(ip);
	struct proc *p = myproc();
	unsigned int mode = ip->mode;
	unsigned int owner = ip->uid;
	unsigned int group = ip->gid;
	int can_exec = 0;

	if (p->uid == owner)
		can_exec = mode & S_IXUSR;
	else if (p->gid == group)
		can_exec = mode & S_IXGRP;
	else
		can_exec = mode & S_IXOTH;

	if (!can_exec) {
		iunlockput(ip);
		end_op();
		return -EPERM;
	}

	iunlockput(ip);
	end_op();

	return exec(path, argv);
}

int sys_chdir(void){
	char *path;
	struct inode *ip;
	struct proc *curproc = myproc();

	begin_op();
	if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
		end_op();
		return -ENOENT;
	}
	ilock(ip);
	if ((ip->mode & S_IFMT) != S_IFDIR) {
		iunlockput(ip);
		end_op();
		return -ENOTDIR;
	}
	iunlock(ip);
	iput(curproc->cwd);
	end_op();
	curproc->cwd = ip;
	return 0;
}

int sys_time(void){
	int esec;
	if (argint(0, &esec) < 0)
		return -EINVAL;

	return epoch_mktime(); // seconds since epoch
}

#define MAJOR(dev) ((dev) >> 8)
#define MINOR(dev) ((dev) & 0xff)

int sys_mknod(void){
	struct inode *ip;
	char *path;
	int mode, dev;

	if (!suser())
		return -EPERM;

	begin_op();
	if((argstr(0, &path)) < 0 || argint(1, &mode) < 0 || argint(2, &dev) < 0){
		end_op();
		return -EACCES;
	}

	if((ip = create(path, mode, MAJOR(dev), MINOR(dev))) == 0){
		end_op();
		return -EEXIST;
	}

	if((mode & S_IFMT) == S_IFBLK)
		ip->size = FSSIZE * BSIZE;

	iunlockput(ip);
	end_op();
	return 0;
}

int sys_chmod(void){
	char *path;
	int mode;
	struct inode *ip;

	if (argstr(0, &path) < 0 || argint(1, &mode) < 0)
		return -EINVAL;

	begin_op();
	if ((ip = namei(path)) == 0) {
		end_op();
		return -ENOENT;
	}

	ilock(ip);
	if (myproc()->uid != 0 && myproc()->uid != ip->uid) {
		iunlock(ip);
		end_op();
		return -EACCES;
	}
	ip->mode = (ip->mode & ~0777) | (mode & 0777);
	iupdate(ip);
	iunlock(ip);
	end_op();

	return 0;
}

int sys_chown(void){
	char *path;
	int owner, group;
	struct inode *ip;

	if (argstr(0, &path) < 0 || argint(1, &owner) < 0 || argint(2, &group) < 0)
		return -EINVAL;

	begin_op();
	if ((ip = namei(path)) == 0) {
		end_op();
		return -ENOENT;
	}

	ilock(ip);
	if (myproc()->uid != 0 && myproc()->uid != ip->uid) {
		iunlock(ip);
		end_op();
		return -EACCES;
	}

	ip->uid = owner;
	ip->gid = group;

	iupdate(ip);
	iunlock(ip);
	end_op();

	return 0;
}

int sys_break(void){
	notim();
	return -1;
}

int sys_stat(void){
	notim();
	return -1;
}

int sys_lseek(void){
	struct file *f;
	int offset;
	int whence;

	if (argfd(0, 0, &f) < 0 || argint(1, &offset) < 0 || argint(2, &whence) < 0)
		return -EINVAL;

	if (f->type == FD_PIPE)
		return -EPERM;

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
			return -ENOENT;
		}

		iunlock(f->ip);
		return f->off;
}

int sys_getpid(void){
	return myproc()->pid;
}

int sys_mount(void){
	notim();
	return -1;
}

int sys_umount(void){
	notim();
	return -1;
}

int sys_setuid(){
	int uid;

	if (argint(0, (int*)&uid) < 0)
		return -EINVAL;

	return(setreuid(uid, uid));
}

int sys_getuid(void){
	return myproc()->uid;
}

int sys_stime(void){
	time_t epoch;

	if (argint(0, (int*)&epoch) < 0)
		return -EINVAL;

	if (!suser())
		return -EPERM;

	tsc_realtime = epoch * 1000000000ULL;
	return 0;
}

int sys_ptrace(void){
	notim();
	return -1;
}

int sys_alarm(void){
	int ticks;
	struct proc *p = myproc();

	if(argint(0, &ticks) < 0)
		return -EINVAL;

	if(ticks < 0)
		return -EPERM;

	int old = p->alarminterval;

	p->alarmticks = ticks;
	p->alarminterval = ticks;

	return old;
}

int sys_fstat(void){
	struct file *f;
	struct stat st;
	struct stat *user_st;

	if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&user_st, sizeof(struct stat)) < 0)
		return -EINVAL;

	// Use memset to ensure padding is zeroed
	memset(&st, 0, sizeof(st));
	stati(f->ip, &st);

	if(copyout(myproc()->pgdir, (unsigned int)user_st, &st, sizeof(st)) < 0)
		return -ENOENT;

	return 0;
}

int sys_pause(void){
	pause();
	return 0;
}

int sys_utime(void){
	char *path;
	struct inode *ip;
	unsigned int now;

	if(argstr(0, &path) < 0)
		return -EINVAL;

	begin_op();

	ip = namei(path);
	if(ip == 0){
		end_op();
		return -ENOENT;
	}

	ilock(ip);
	now = epoch_mktime();
	ip->ctime = now;
	ip->lmtime = now;
	iupdate(ip);
	iunlock(ip);
	end_op();
	return 0;
}

int sys_stty(void){
	struct ttyb *uttyb;
	if (argptr(0, (char **)&uttyb, sizeof(struct ttyb)) < 0)
		return -EINVAL;

	acquire(&cons.lock);
	ttyb.speeds = uttyb->speeds;
	ttyb.tflags = uttyb->tflags;
	ttyb.erase = uttyb->erase;
	ttyb.kill = uttyb->kill;
	release(&cons.lock);
	return 0;
}

int sys_gtty(void){
	struct ttyb *uttyb;
	if (argptr(0, (char **)&uttyb, sizeof(struct ttyb)) < 0)
		return -EINVAL;

	acquire(&cons.lock);
	*uttyb = ttyb;
	release(&cons.lock);
	return 0;
}

int sys_access(void){
	char *path;
	int mode;
	struct inode *ip;
	int perm;
	int uid = myproc()->uid;
	int gid = myproc()->gid;

	if (argstr(0, &path) < 0 || argint(1, &mode) < 0)
		return -EINVAL;

	mode &= 0007;

	begin_op();
	ip = namei(path);
	if (!ip) {
		end_op();
		return -EBADF;
	}

	ilock(ip);

	perm = ip->mode & 0777;
	if (uid == ip->uid)
		perm >>= 6;
	else if (gid == ip->gid)
		perm >>= 3;

	if ((perm & mode) == mode) {
		iunlock(ip);
		end_op();
		return 0;
	}

	iunlock(ip);
	end_op();
	return -EACCES;
}

int sys_nice(void){
	notim();
	return -1;
}

int sys_ftime(void){
	notim();
	return -1;
}

int sys_sync(void){
	sync();
	return 0;
}

int sys_kill(void){
	int pid, signo;

	if(argint(0, &pid) < 0 || argint(1, &signo) < 0)
		return -EINVAL;

	if(signo < 1 || signo >= NSIG)
		return -EINVAL;

	return kill(pid, signo);
}

int sys_rename(void){
	notim();
	return -1;
}

int sys_mkdir(void){
	char *path;
	struct inode *ip;

	begin_op();
	if(argstr(0, &path) < 0 || (ip = create(path, (S_IFDIR | S_IRUSR | S_IXUSR | S_IWUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) & ~myproc()->umask, 0, 0)) == 0){
		end_op();
		return -EEXIST;
	}
	iunlockput(ip);
	end_op();
	return 0;
}

int sys_rmdir(void){
	struct inode *ip, *dp;
	struct dirent de;
	char name[DIRSIZ], *path;
	unsigned int off;

	if(argstr(0, &path) < 0)
		return -EINVAL;

	begin_op();
	if((dp = nameiparent(path, name)) == 0){
		end_op();
		return -ENOENT;
	}

	ilock(dp);

	if (myproc()->uid != dp->uid) {
		iunlockput(dp);
		end_op();
		return -EPERM;
	}

	if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0) {
		iunlockput(dp);
		end_op();
		return -EPERM;
	}

	if((ip = dirlookup(dp, name, &off)) == 0) {
		iunlockput(dp);
		end_op();
		return -ENOENT;
	}
	ilock(ip);

	if(!S_ISDIR(ip->mode)) {
		iunlockput(ip);
		iunlockput(dp);
		end_op();
		return -ENOTDIR;
	}

	if(!isdirempty(ip)) { // ensure the directory is empty
		iunlockput(ip);
		iunlockput(dp);
		end_op();
		return -ENOTEMPTY;
	}

	memset(&de, 0, sizeof(de));
	if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
		panic("rmdir: writei");

	dp->nlink--;
	iupdate(dp);
	iunlockput(dp);

	ip->nlink--;
	iupdate(ip);
	iunlockput(ip);

	end_op();

	return 0;
}

int sys_dup(void){
	struct file *f;
	int fd;

	if(argfd(0, 0, &f) < 0)
		return -EINVAL;

	if((fd=fdalloc(f)) < 0)
		return -ENOENT;

	filedup(f);
	return fd;
}

int sys_pipe(void){
	int *fd;
	struct file *rf, *wf;
	int fd0, fd1;

	if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
		return -EINVAL;

	if(pipealloc(&rf, &wf) < 0)
		return -ENOMEM;

	fd0 = -1;
	if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
		if(fd0 >= 0)
			myproc()->ofile[fd0] = 0;
		fileclose(rf);
		fileclose(wf);
		return -ENOMEM;
	}
	fd[0] = fd0;
	fd[1] = fd1;
	return 0;
}

int sys_times(void){
	notim();
	return -1;
}

int sys_prof(void){
	notim();
	return -1;
}

/*
 * change the location of the program break (and return the new address)
 */
int sys_brk(void){
	int addr;
	int old;
	int delta;

	if(argint(0, &addr) < 0)
		return -EINVAL;

	old = myproc()->sz;

	if(addr == 0)
		return old;

	if(addr < 0)
		return -EINVAL;

	delta = addr - old;

	if(delta != 0) {
		if(growproc(delta) < 0)
			return -ENOMEM;
	}

	return addr;
}

int sys_setgid(void){
	int gid;

	if (argint(0, &gid) < 0)
		return -EINVAL;

	return setregid(gid, gid);
}

int sys_getgid(void){
	return myproc()->gid;
}

int sys_signal(void){
	int signum;
	unsigned int handler;
	struct proc *p = myproc();

	if(argint(0, &signum) < 0 || argint(1, (int*)&handler) < 0)
		return -EINVAL;

	if(signum < 1 || signum >= NSIG || signum == SIGKILL)
		return -EPERM;  // Can't catch SIGKILL

	unsigned int old = p->sighandlers[signum];
	p->sighandlers[signum] = handler;

	return old;
}

int sys_geteuid(void){
	return myproc()->euid;
}

int sys_getegid(void){
	return myproc()->egid;
}

int sys_acct(void){
	notim();
	return -1;
}

int sys_phys(void){
	notim();
	return -1;
}

int sys_lock(void){
	notim();
	return -1;
}

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

/*
 * helper for ioctl, effectively a placeholder, but oh well
 */
int tty_get_winsize(struct winsize *ws) {
	if (!ws) return -1;

	ws->ws_row = 25;
	ws->ws_col = 79;
	ws->ws_xpixel = 720;
	ws->ws_ypixel = 400;

	return 0;
}

/*
 * palceholder for IOCTL to get printf working under MUSL
 */
int sys_ioctl(void){
	struct file *f;
	int req;
	void *arg;

	if(argfd(0, 0, &f) < 0 || argint(1, &req) < 0 || argptr(2, (char**)&arg, sizeof(struct winsize)) < 0)
		return -EINVAL;

	switch(req){
		case TIOCGWINSZ: // 21523 / 0x5413 winsize
			return tty_get_winsize((struct winsize*)arg);
		default:
			printk("%s: bad ioctl request [0x%x]\n", myproc()->name, req);
			return -ENOTTY;
	}
}

int sys_fcntl(void){
	int fd;
	int cmd;
	unsigned long arg;
	struct file *f;
	struct proc *curproc = myproc();

	if(argfd(0, &fd, &f) < 0 || argint(1, &cmd) < 0 || argint(2, (int*)&arg) < 0)
		return -EINVAL;

	switch(cmd){
		case F_DUPFD:
		case F_DUPFD_CLOEXEC:
			// find the lowest fd >= arg
			for(int i = arg; i < NOFILE; i++){
				if(curproc->ofile[i] == 0){
					curproc->ofile[i] = f;
					filedup(f);
					if(cmd == F_DUPFD_CLOEXEC){
						curproc->cloexec[i] = 1;
					}
					return i;
				}
			}
			return -EMFILE;

		case F_GETFL:
			return f->flags;
		case F_SETFL:
			// only allow changing O_APPEND for now
			if(arg & ~O_APPEND)
				return -EPERM;
			f->flags = (f->flags & ~O_APPEND) | (arg & O_APPEND);
			return 0;
		default:
			return -EPERM;
	}
}

int sys_mpx(void){
	notim();
	return -1;
}

int sys_ulimit(void){
	notim();
	return -1;
}

int sys_sbrk(void){
	int addr;
	int n;

	if(argint(0, &n) < 0)
		return -EINVAL;

	addr = myproc()->sz;
	if(growproc(n) < 0)
		return -ENOMEM;

	return addr;
}

int sys_umask(void){
	mode_t mask;
	mode_t old;

	if(argptr(0, (void*)&mask, sizeof(mode_t)) < 0)
		return -EINVAL;
	old = myproc()->umask;
	myproc()->umask = mask & 0777;
	return old;
}

int sys_chroot(void){
	notim();
	return -1;
}

int sys_ustat(void){
	notim();
	return -1;
}

int sys_dup2(void){
	int oldfd, newfd;
	struct file *f;
	struct proc *p = myproc();

	if(argint(0, &oldfd) < 0 || argint(1, &newfd) < 0)
		return -EINVAL;

	if(oldfd < 0 || oldfd >= NOFILE || newfd < 0 || newfd >= NOFILE)
		return -EBADF;

	f = p->ofile[oldfd];
	if(f == 0)
		return -EBADF;

	if(oldfd == newfd)
		return newfd;

	if(p->ofile[newfd]){
		fileclose(p->ofile[newfd]);
		p->ofile[newfd] = 0;
	}

	p->ofile[newfd] = f;
	filedup(f);

	p->cloexec[newfd] = 0;

	return newfd;
}

int sys_getppid(void){
	return myproc()->parent->pid;
}

int sys_getpgrp(void){
	return myproc()->pgrp;
}

int sys_setsid(void){
	struct proc *p = myproc();

	if (p->leader && !suser())
		return -EPERM;

	p->leader = 1;
	p->session = p->pid;
	p->pgrp = p->pid;
	p->tty = -1;
	return p->pgrp;
}

int sys_sigaction(void){
	notim();
	return -1;
}

int sys_sgetmask(void){
	return -ENOSYS;
}
int sys_ssetmask(void){
	return -ENOSYS;
}

int sys_setreuid(){
	int ruid, euid;

	if(argint(0, &ruid) < 0 || argint(1, &euid) < 0)
		return -EINVAL;

	return setreuid(ruid, euid);
}

int sys_setregid(){
	int rgid, egid;

	if(argint(0, &rgid) < 0 || argint(1, &egid) < 0)
		return -EINVAL;

	return setregid(rgid, egid);
}

int sys_sethostname(void){
	const char * hostname;
	size_t len;

	if (!suser())
		return -EPERM;

	if(argstr(0, (char **)&hostname) < 0 || argptr(1, (void*)&len, sizeof(len)) < 0)
		return -EINVAL;

	strncpy(sys_nodename, hostname, len);
	sys_nodename[len] = '\0';
	return 0;
}

struct rusage {
	struct timespec64 ru_utime; /* user CPU time used */
	struct timespec64 ru_stime; /* system CPU time used */
	long   ru_maxrss;	/* maximum resident set size */
	long   ru_ixrss;	 /* integral shared memory size */
	long   ru_idrss;	 /* integral unshared data size */
	long   ru_isrss;	 /* integral unshared stack size */
	long   ru_minflt;	/* page reclaims (soft page faults) */
	long   ru_majflt;	/* page faults (hard page faults) */
	long   ru_nswap;	 /* swaps */
	long   ru_inblock;       /* block input operations */
	long   ru_oublock;       /* block output operations */
	long   ru_msgsnd;	/* IPC messages sent */
	long   ru_msgrcv;	/* IPC messages received */
	long   ru_nsignals;      /* signals received */
	long   ru_nvcsw;	 /* voluntary context switches */
	long   ru_nivcsw;	/* involuntary context switches */
};

/*
 * This is effectively a placeholder
 */
int sys_getrusage(void){
	int pid;
	struct rusage *uru;
	struct rusage ru;

	if(argint(0, &pid) < 0 || argptr(1, (void*)&uru, sizeof(uru)) < 0)
		return -EINVAL;

	struct proc *pp = findproc(pid, myproc());
	if(!pp)
		return -ESRCH;

	ru.ru_utime.tv_sec = 0;
	ru.ru_stime.tv_nsec = 0;
	ru.ru_maxrss = 0;
	ru.ru_ixrss = 0;
	ru.ru_idrss = 0;
	ru.ru_isrss = 0;
	ru.ru_minflt = 0;
	ru.ru_majflt = 0;
	ru.ru_nswap = 0;
	ru.ru_inblock = 0;
	ru.ru_oublock = 0;
	ru.ru_msgsnd = 0;
	ru.ru_msgrcv = 0;
	ru.ru_nsignals = 0;
	ru.ru_nvcsw = 0;
	ru.ru_nivcsw = 0;

	if(copyout(myproc()->pgdir, (unsigned int)uru, &ru, sizeof(ru)) < 0)
		return -EFAULT;

	return 0;
}

/*
 * Supplementary group IDs
 */
int sys_setgroups(void){
	int gidsetsize;
	gid_t *ugroups;
	struct proc *p = myproc();

	if (!suser())
		return -EPERM;

	if(argint(0, &gidsetsize) < 0 || argptr(1, (void*)&ugroups, gidsetsize * sizeof(gid_t)) < 0)
		return -EINVAL;

	if(gidsetsize < 0 || gidsetsize > NGROUPS)
		return -EINVAL;

	memmove(p->groups, ugroups, gidsetsize * sizeof(gid_t));
	return 0;
}

int sys_symlink(void){
	return sys_link();
}

int sys_reboot(void){
	int magic;
	int magic_too;
	int flag;

	if (!suser())
		return -EPERM;

	if(argint(0, &magic) < 0 || argint(1, &magic_too) < 0 || argint(2, &flag) < 0)
		return -EINVAL;

	if (magic != 0xfee1dead || magic_too != 672274793)
		return -EINVAL;

	if (flag == 0x01234567)
		hard_reset_now();
	else if (flag == 0x89ABCDEF)
		C_A_D = 1;
	else if (!flag)
		C_A_D = 0;
	else
		return -EINVAL;

	return 0;
}

/*
 * Pretty much a placeholder
 */
int sys_idle(void){
	asm volatile("sti\n"
		     "hlt");
	return 0;
}

int sys_wait4(void){
	int pid;
	int *status;
	int options;

	if(argint(0, &pid) < 0 || argptr(1, (void*)&status, sizeof(*status)) < 0 || argint(2, &options) < 0)
		return -EINVAL;

	return waitpid(pid, status, options);
}

int sys_rt_sigreturn(void);

int sys_sigreturn(void){
	return sys_rt_sigreturn();
}

int sys_setdomainname(void){
	const char * domainname;
	size_t len;

	if (!suser())
		return -EPERM;

	if(argstr(0, (char **)&domainname) < 0 || argptr(1, (void*)&len, sizeof(len)) < 0)
		return -EINVAL;

	strncpy(sys_domainname, domainname, len);
	sys_domainname[len] = '\0';
	return 0;
}

int sys_uname(void){
	struct utsname *u;

	if(argptr(0, (char**)&u, sizeof(struct utsname)) < 0)
		return -EINVAL;

	safestrcpy(u->sysname, sys_name, sizeof(u->sysname));
	safestrcpy(u->nodename, sys_nodename, sizeof(u->nodename));
	safestrcpy(u->release, sys_release, sizeof(u->release));
	safestrcpy(u->version, sys_version, sizeof(u->version));
	safestrcpy(u->machine, "i386", sizeof(u->machine));
	safestrcpy(u->domainname, sys_domainname, sizeof(u->domainname));
	return 0;
}

int sys_getpgid(void){
	return myproc()->parent->gid;
}

int sys_getdents(void){
	int fd;
	int count;
	struct dirent *dp;
	struct file *f;
	struct inode *ip;
	int n = 0;
	struct dirent de;

	if(argint(0, &fd) < 0 || argptr(1, (void*)&dp, sizeof(*dp)) < 0 || argint(2, &count) < 0)
		return -EINVAL;

	if(fd < 0 || fd >= NOFILE)
		return -1;

	f = myproc()->ofile[fd];
	if(f == 0)
		return -1;

	ip = f->ip;
	if(ip == 0 || !(ip->mode & S_IFDIR))
		return -1;

	ilock(ip);

	while(f->off < ip->size){
		if(count - n < sizeof(struct dirent))
			break;

		if(readi(ip, (char*)&de, f->off, sizeof(de)) != sizeof(de))
			break;
		f->off += sizeof(de);

		if(de.d_ino == 0)
			continue;
		de.d_off = f->off;
		de.d_reclen = sizeof(struct dirent);

		if(copyout(myproc()->pgdir, (unsigned int)dp + n, (char*)&de, sizeof(de)) < 0){
			iunlock(ip);
			return -1;
		}

		n += sizeof(struct dirent);
	}

	iunlock(ip);
	return n;
}

/*
 * multiple buffer write
 */
int sys_writev(void){
	struct file *f;
	int count;
	unsigned int *vec;
	int i;
	int total;

	if(argfd(0, 0, &f) < 0 || argint(2, &count) < 0 || argptr(1, (char**)&vec, count * sizeof(unsigned int) * 2) < 0)
		return -EINVAL;

	total = 0;
	for(i = 0; i < count; i++){
		char *p = (char*)vec[i * 2];
		int n = (int)vec[i * 2 + 1];
		int r = filewrite(f, p, n);
		if(r < 0)
			return -EIO;
		total += r;
	}
	return total;
}

int sys_nanosleep(void){
	return -ENOSYS;
}

int sys_setresuid(void){
	int uid, euid, suid;
	struct proc *p = myproc();

	if (argint(0, &uid) < 0 || argint(1, &euid) < 0 || argint(2, &suid) < 0)
		return -EINVAL;

	if (uid < -1 || euid < -1 || suid < -1)
		return -EPERM;

	if (p->uid != 0) {
		if (uid != -1 && uid != p->uid && uid != p->euid)
			return -EPERM;
		if (euid != -1 && euid != p->uid && euid != p->suid)
			return -EPERM;
		if (suid != -1 && suid != p->euid)
			return -EPERM;
	}

	if (uid != -1)
		p->uid = uid;
	if (euid != -1)
		p->euid = euid;
	if (suid != -1)
		p->suid = suid;
	else if (euid != -1)
		p->suid = p->euid;

	return 0;
}

int sys_setresgid(void){
	int gid, egid, sgid;
	struct proc *p = myproc();

	if (argint(0, &gid) < 0 || argint(1, &egid) < 0 || argint(2, &sgid) < 0)
		return -EINVAL;

	if (gid < -1 || egid < -1 || sgid < -1)
		return -EPERM;

	if (p->gid != 0) {
		if (gid != -1 && gid != p->gid && gid != p->egid)
			return -EPERM;
		if (egid != -1 && egid != p->gid && egid != p->sgid)
			return -EPERM;
		if (sgid != -1 && sgid != p->egid)
			return -EPERM;
	}

	if (gid != -1)
		p->gid = gid;
	if (egid != -1)
		p->egid = egid;
	if (sgid != -1)
		p->sgid = sgid;
	else if (egid != -1)
		p->sgid = p->egid;

	return 0;
}

/*
 * return signal
 */
int
sys_rt_sigreturn(void)
{
	struct proc *p = myproc();

	// Restore the trapframe that was saved by dosignal
	if(p->saved_trapframe_sp) {
		struct trapframe *saved = (struct trapframe*)p->saved_trapframe_sp;
		*p->tf = *saved;
		p->saved_trapframe_sp = 0;
	}

	return p->tf->eax;
}

/*
 * signal mask
 */
int sys_rt_sigprocmask(void){
	int how;
	unsigned int *set;
	unsigned int *oldset;
	size_t sigsetsize;

	if(argint(0, &how) < 0 || argptr(1, (void*)&set, sizeof(*set)) < 0 || argptr(2, (void*)&oldset, sizeof(*oldset)) < 0 || argint(3, (int*)&sigsetsize) < 0)
		return -EINVAL;

	struct proc *curproc = myproc();

	// return old mask if requested
	if(oldset != 0)
		*oldset = curproc->sigmask;

	// apply new mask if provided
	if(set != 0) {
		unsigned int newmask = *set;
		newmask &= ~((1 << SIGKILL) | (1 << SIGSTOP));
		switch(how) {
			case SIG_BLOCK:
				curproc->sigmask |= newmask;
				break;
			case SIG_UNBLOCK:
				curproc->sigmask &= ~newmask;
				break;
			case SIG_SETMASK:
				curproc->sigmask = newmask;
				break;
			default:
				return -EPERM;
		}
	}

	return 0;
}

int sys_rt_sigaction(void){
	int signum;
	struct {
		unsigned int sa_handler;
		unsigned int sa_flags;
		unsigned int sa_restorer;
		unsigned int sa_mask;
	} *act;
	unsigned int old;

	if(argint(0, &signum) < 0 || argptr(1, (void*)&act, sizeof(*act)) < 0 || argint(2, (int*)&old) < 0)
		return -EINVAL;

	if(signum < 1 || signum >= NSIG || signum == SIGKILL)
		return -EPERM;

	struct proc *p = myproc();

	if(old)
		*(unsigned int*)old = p->sighandlers[signum];

	if(act) {
		p->sighandlers[signum] = act->sa_handler;
		p->sigrestorers[signum] = act->sa_restorer;
	}

	return 0;
}

/*
 * SYS_rt_sigsuspend is in proc.c
 */

/*
 * build a path from the proc's cwd
 */
int sys_getcwd(void){
	char *buf;
	size_t size;

	if(argptr(0, (void*)&buf, sizeof(*buf)) < 0 || argint(1, (int*)&size) < 0)
		return -EINVAL;

	struct proc *curproc = myproc();
	struct inode *cwd = curproc->cwd;
	char path[DIRSIZ];
	int pos = DIRSIZ - 1;
	path[pos] = '\0';

	ilock(cwd);
	struct inode *root = namei("/");

	while(cwd->inum != root->inum) {
		struct inode *parent = dirlookup(cwd, "..", 0);
		if(!parent) break;

		ilock(parent);
		struct dirent de;
		for(unsigned int off = 0; off < parent->size; off += sizeof(de)) {
			readi(parent, (char*)&de, off, sizeof(de));
			if(de.d_ino == cwd->inum) {
				int len = strlen(de.d_name);
				pos -= len + 1;
				path[pos] = '/';
				memmove(&path[pos + 1], de.d_name, len);
				break;
			}
		}
		iunlock(cwd);
		cwd = parent;
	}
	iunlock(cwd);
	iput(root);

	if(path[pos] == '\0') path[--pos] = '/';

	memmove(buf, &path[pos], 128 - pos);
	return (int)buf;
}

int sys_vfork(void){
	return sys_fork();
}

int sys_getuid32(void){
	return myproc()->uid;
}

int sys_getgid32(void){
	return myproc()->gid;
}

int sys_geteuid32(void){
	return myproc()->euid;
}

int sys_getegid32(void){
	return myproc()->egid;
}

int sys_setgroups32(void){
	return sys_setgroups();
}

int sys_setresuid32(void){
	return sys_setresuid();
}

int sys_setresgid32(void){
	return sys_setresgid();
}

int sys_setuid32(void){
	return sys_setuid();
}

int sys_setgid32(void){
	return sys_setgid();
}

int sys_getdents64(void){
	return sys_getdents();
}

int sys_fcntl64(void){
	return sys_fcntl();
}

/*
 * we aint got no threads
 */
int sys_exit_group(void){
	int status;

	if(argint(0, &status) < 0)
		return -EINVAL;

	exit(status);
	return 0;
}

/*
 * this syscall SHOULD return the thread ID, but we don't got that.
 * return PID.
 */
int sys_set_tid_address(void){
	int *tidptr;

	if(argptr(0, (void*)&tidptr, sizeof(*tidptr)) < 0)
		return -EINVAL;

	return myproc()->pid;
}

int sys_clock_settime32(void){
	int clkid;
	struct timespec64 *utp;

	if (!suser())
		return -EPERM;

	if(argint(0, &clkid) < 0 || argptr(1, (void*)&utp, sizeof(*utp)) < 0)
		return -EINVAL;

	if(clkid != CLOCK_REALTIME)
		return -EPERM;

	tsc_realtime = utp->tv_sec * 1000000000ULL - tsc_offset;
	return 0;
}

int sys_clock_settime64(void){
	return sys_clock_settime32();
}

int sys_clock_gettime(void){
	int clkid;
	struct timespec64 *utp;
	uint64_t tsc_delta, ns, tsc_now;

	if(argint(0, &clkid) < 0 || argptr(1, (void*)&utp, sizeof(*utp)) < 0)
		return -EINVAL;

	tsc_now = rdtsc();
	tsc_delta = tsc_now - tsc_offset;

	uint64_t seconds = tsc_delta / tsc_freq_hz;
	uint64_t remainder = tsc_delta % tsc_freq_hz;
	ns = seconds * 1000000000ULL + (remainder * 1000000000ULL) / tsc_freq_hz;

	switch(clkid) {
		case CLOCK_MONOTONIC:
			break;
		case CLOCK_REALTIME:
			ns += tsc_realtime;
			break;
		default:
			return -EPERM;
	}

	struct timespec64 ts;
	ts.tv_sec = ns / 1000000000ULL;
	ts.tv_nsec = ns % 1000000000ULL;

	if(copyout(myproc()->pgdir, (unsigned int)utp, &ts, sizeof(ts)) < 0)
		return -EINVAL;
	return 0;
}

//       int linkat(int olddirfd, const char *oldpath,
//		  int newdirfd, const char *newpath, int flags);

/*
 * Pretty much a placeholder for the LN utility
 * to work correctly. sys_linkat is like sys_link
 * but doesn't allow directories, and should
 * probably accept flags.
 */
int sys_linkat(void){
	char name[DIRSIZ], *new, *old;
	struct inode *dp, *ip;

	if(argstr(1, &old) < 0 || argstr(3, &new) < 0)
		return -EINVAL;

	begin_op();
	if((ip = namei(old)) == 0){
		end_op();
		return -ENOENT;
	}

	ilock(ip);

	if(S_ISDIR(ip->mode)){
		iunlockput(ip);
		end_op();
		return -EPERM;
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
	return -ENOENT;
}

int sys_statx(void){
	int dirfd;
	char *pathname;
	int flags;
	unsigned int mask;
	struct statx *user_statxbuf;
	struct statx stxbuf;
	struct inode *ip;
	struct stat st;

	if(argint(0, &dirfd) < 0 || argstr(1, &pathname) < 0 || argint(2, &flags) < 0 || argint(3, (int*)&mask) < 0 || argptr(4, (char**)&user_statxbuf, sizeof(struct statx)) < 0)
		return -EINVAL;

	if((ip = namei(pathname)) == 0)
		return -ENOENT;

	ilock(ip);

	memset(&st, 0, sizeof(st));
	stati(ip, &st);
	memset(&stxbuf, 0, sizeof(stxbuf));

	stxbuf.stx_mask = mask;
	stxbuf.stx_blksize = BSIZE;
	stxbuf.stx_nlink = st.st_nlink;
	stxbuf.stx_uid = st.st_uid;
	stxbuf.stx_gid = st.st_gid;
	stxbuf.stx_mode = st.st_mode;
	stxbuf.stx_ino = st.st_ino;
	stxbuf.stx_size = st.st_size;
	stxbuf.stx_blocks = (st.st_size + BSIZE -1) / BSIZE;
	stxbuf.stx_ctime.tv_sec = st.st_ctime;
	stxbuf.stx_ctime.tv_nsec = 0;
	stxbuf.stx_mtime.tv_sec = st.st_lmtime;
	stxbuf.stx_mtime.tv_nsec = 0;
	stxbuf.stx_btime.tv_sec = st.st_ctime;
	stxbuf.stx_btime.tv_nsec = 0;
	stxbuf.stx_atime.tv_sec = st.st_lmtime;
	stxbuf.stx_atime.tv_nsec = 0;
	stxbuf.stx_dev_major = st.st_dev;
	stxbuf.stx_dev_minor = 0;


	iunlockput(ip);

	if(copyout(myproc()->pgdir, (unsigned int)user_statxbuf, &stxbuf, sizeof(stxbuf)) < 0)
		return -EINVAL;

	return 0;
}

int sys_clock_gettime64(void){
	return sys_clock_gettime();
}
