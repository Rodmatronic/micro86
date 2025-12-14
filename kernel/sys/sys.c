//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

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

// sysfile.c

int
sys_utime(void)
{
	char *path;
	struct inode *ip;
	unsigned int now;

	if(argstr(0, &path) < 0) {
		errno=EPERM;
		return -1;
	}

	begin_op();

	ip = namei(path);
	if(ip == 0){
		end_op();
		errno=ENOENT;
		return -1;
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

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int
sys_sync(void)
{
	sync();
	return 0;
}

static int
argfd(int n, int *pfd, struct file **pf)
{
	int fd;
	struct file *f;

	if(argint(n, &fd) < 0){
		errno=EPERM;
		return -1;
	}
	if(fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0){
		errno=ENOENT;
		return -1;
	}
	if(pfd)
		*pfd = fd;
	if(pf)
		*pf = f;
	return 0;
}

int
sys_chmod(void)
{
	char *path;
	int mode;
	struct inode *ip;

	if (argstr(0, &path) < 0 || argint(1, &mode) < 0){
		errno=EPERM;
		return -1;
	}

	begin_op();
	if ((ip = namei(path)) == 0) {
		end_op();
		errno=ENOENT;
		return -1;
	}

	ilock(ip);
	if (myproc()->uid != 0 && myproc()->uid != ip->uid) {
		iunlock(ip);
		end_op();
		errno=EACCES;
		return -1;
	}
	ip->mode = (ip->mode & ~0777) | (mode & 0777);
	iupdate(ip);
	iunlock(ip);
	end_op();

	return 0;
}

int
sys_chown(void)
{
	char *path;
	int owner, group;
	struct inode *ip;

	// Fetch syscall arguments: path, uid, gid
	if (argstr(0, &path) < 0 || argint(1, &owner) < 0 || argint(2, &group) < 0){
		errno=EPERM;
		return -1;
	}

	begin_op();
	if ((ip = namei(path)) == 0) {
		end_op();
		errno=ENOENT;
		return -1;
	}

	ilock(ip);
	if (myproc()->uid != 0 && myproc()->uid != ip->uid) {
		iunlock(ip);
		end_op();
		errno=EACCES;
		return -1;
	}

	ip->uid = owner;
	ip->gid = group;

	iupdate(ip);
	iunlock(ip);
	end_op();

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
		argint(2, &whence) < 0){
		errno=EPERM;
		return -1;
	}
	if (f->type == FD_PIPE) {
		errno=EPERM;
		return -1;
	}

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
			errno=ENOENT;
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

	if(argfd(0, 0, &f) < 0) {
		errno=EPERM;
		return -1;
	}
	if((fd=fdalloc(f)) < 0){
		errno=ENOENT;
		return -1;
	}
	filedup(f);
	return fd;
}

int
sys_read(void)
{
	struct file *f;
	int n;
	char *p;

	if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0){
		errno=EPERM;
		return -1;
	}
	return fileread(f, p, n);
}

int
sys_write(void)
{
	struct file *f;
	int n;
	char *p;

	if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0){
		errno=EPERM;
		return -1;
	}
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
		argptr(1, (void*)&user_st, sizeof(struct stat)) < 0){
		errno=EPERM;
		return -1;
		}

	// Use memset to ensure padding is zeroed
	memset(&st, 0, sizeof(st));

	stati(f->ip, &st);

	// Copy entire struct including padding
	if(copyout(myproc()->pgdir, (unsigned int)user_st, &st, sizeof(st)) < 0){
		errno=ENOENT;
		return -1;
	}

	return 0;
}

// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
	char name[DIRSIZ], *new, *old;
	struct inode *dp, *ip;

	if(argstr(0, &old) < 0 || argstr(1, &new) < 0){
		errno=EPERM;
		return -1;
	}

	begin_op();
	if((ip = namei(old)) == 0){
		end_op();
		errno=ENOENT;
		return -1;
	}

	ilock(ip);
	if ((ip->mode & S_IFMT) != S_IFDIR){
		iunlockput(ip);
		end_op();
		errno=EISDIR;
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
	errno=ENOENT;
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
		if(de.d_ino != 0)
			return 0;
	}
	return 1;
}

int
sys_unlink(void)
{
	struct inode *ip, *dp;
	struct dirent de;
	char name[DIRSIZ], *path;
	unsigned int off;

	if(argstr(0, &path) < 0){
		errno=EPERM;
		return -1;
	}

	begin_op();
	if((dp = nameiparent(path, name)) == 0){
		end_op();
		errno=ENOENT;
		return -1;
	}

	ilock(dp);

	if (myproc()->uid != dp->uid &&
			((dp->mode & S_IFMT) != S_IFCHR) &&
			((dp->mode & S_IFMT) != S_IFBLK)) {
		iunlock(dp);
		end_op();
		errno=EACCES;
		return -1;
	}

	// Cannot unlink "." or "..".
	if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0) {
		errno=EPERM;
		goto bad;
	}

	if((ip = dirlookup(dp, name, &off)) == 0) {
		errno=ENOENT;
		goto bad;
	}
	ilock(ip);

	if(ip->nlink < 1)
		panic("unlink: nlink < 1");
	if(((ip->mode & S_IFMT) == S_IFDIR) && !isdirempty(ip)) {
		iunlockput(ip);
		errno=ENOENT;
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

	if(type == S_IFDIR){	// Create . and .. entries.
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

int
sys_open(void)
{
	char *path;
	int fd, omode;
	int mode = 0;
	struct file *f;
	struct inode *ip;
	//struct proc *p = myproc();

	if(argstr(0, &path) < 0 || argint(1, &omode) < 0){
		errno=EPERM;
		return -1;
	}

	if (omode & O_CREAT) {
		argint(2, &mode);
	}


	begin_op();

	if(omode & O_CREAT){
		if (mode == 2)
			mode = 0666;
		ip = create(path, S_IFREG | mode, 0, 0);
		if(ip == 0){
			end_op();
			errno=ENOENT;
			return -2;
		}
	} else {
		if((ip = namei(path)) == 0){
			end_op();
			errno=ENOENT;
			return -2;
		}
		ilock(ip);

		if (((ip->mode & S_IFMT) == S_IFDIR) && omode != O_RDONLY) {
			iunlockput(ip);
			end_op();
			errno=EACCES;
			return -1;
		}
	}
	if (omode & O_TRUNC) {
		itrunc(ip);
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

int
sys_mkdir(void)
{
	char *path;
	struct inode *ip;

	begin_op();
	if(argstr(0, &path) < 0 || (ip = create(path, S_IFDIR | S_IRUSR | S_IXUSR | S_IWUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH, 0, 0)) == 0){
		end_op();
		errno=EEXIST;
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
		(ip = create(path, S_IFCHR | S_IRUSR | S_IWUSR, major, minor)) == 0){
		end_op();
		errno=EACCES;
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
		errno=ENOENT;
		return -1;
	}
	ilock(ip);
	if ((ip->mode & S_IFMT) != S_IFDIR) {
		iunlockput(ip);
		end_op();
		errno=EISDIR;
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
	unsigned int uargv, uarg;

	if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
		return -1;
	}
	memset(argv, 0, sizeof(argv));
	for(i=0;; i++){
		if(i >= NELEM(argv)){
			return -1;
		}
		if(fetchint(uargv+4*i, (int*)&uarg) < 0){
			return -12;
		}
		if(uarg == 0){
			argv[i] = 0;
			break;
		}
		if(fetchstr(uarg, &argv[i]) < 0){
			return -1;
		}
	}
	struct inode *ip;
	begin_op();
	ip = namei(path);
	if(ip == 0){
		end_op();
		return -2;
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
		return -1;
	}

	iunlockput(ip);
	end_op();

	return exec(path, argv);
}

int
sys_pipe(void)
{
	int *fd;
	struct file *rf, *wf;
	int fd0, fd1;

	if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0){
		errno=EPERM;
		return -1;
	}
	if(pipealloc(&rf, &wf) < 0){
		errno=ENOMEM;
		return -1;
	}
	fd0 = -1;
	if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
		if(fd0 >= 0)
			myproc()->ofile[fd0] = 0;
		fileclose(rf);
		fileclose(wf);
		errno=ENOMEM;
		return -1;
	}
	fd[0] = fd0;
	fd[1] = fd1;
	return 0;
}

extern struct ttyb ttyb;
extern struct cons cons;

int
sys_gtty(void)
{
	struct ttyb *uttyb;
	if (argptr(0, (char **)&uttyb, sizeof(struct ttyb)) < 0) {
		errno=EPERM;
		return -1;
	}

	acquire(&cons.lock);
	*uttyb = ttyb;
	release(&cons.lock);
	return 0;
}

int
sys_stty(void)
{
	struct ttyb *uttyb;
	if (argptr(0, (char **)&uttyb, sizeof(struct ttyb)) < 0) {
		errno=EPERM;
		return -1;
	}

	acquire(&cons.lock);
	ttyb.speeds = uttyb->speeds;
	ttyb.tflags = uttyb->tflags;
	ttyb.erase = uttyb->erase;
	ttyb.kill = uttyb->kill;
	release(&cons.lock);
	return 0;
}

char sys_nodename[65] = "localhost";

int
sys_uname(void)
{
	struct utsname *u;

	if(argptr(0, (char**)&u, sizeof(struct utsname)) < 0){
		errno=EPERM;
		return -1;
	}

	safestrcpy(u->sysname, sys_name, sizeof(u->sysname));
	safestrcpy(u->nodename, sys_nodename, sizeof(u->nodename));
	safestrcpy(u->release, sys_release, sizeof(u->release));
	safestrcpy(u->version, sys_version, sizeof(u->version));
	safestrcpy(u->machine, "i386", sizeof(u->machine));
	safestrcpy(u->domainname, "domainname", sizeof(u->domainname));
	return 0;
}

int sys_oldolduname(void)
{
	return sys_uname();
}

int sys_stime(void) {
	unsigned long epoch;
	if (argint(0, (int*)&epoch) < 0) {
		errno=EPERM;
		return -1;
	}
	struct proc *p = myproc();
	if (p->uid != 0) return 1; // Operation not permitted
	set_kernel_time((unsigned long)epoch);
	return 0;
}

// seconds since epoch
int
sys_time(void)
{
	int esec;
	if (argint(0, &esec) < 0) {
		errno=EPERM;
		return -1;
	}
	return epoch_mktime();
}

int
sys_setgid(void) {
	int gid;
	if (argint(0, &gid) < 0) {
		errno=EPERM;
		return -1;
	}

	struct proc *p = myproc();
	p->gid = p->egid = p->sgid = gid;
	return 0;
}

int
sys_setuid(void) {
	int uid;
	if (argint(0, &uid) < 0) {
		errno=EPERM;
		return -1;
	}

	struct proc *p = myproc();
	p->uid = p->euid = p->suid = uid;
	return 0;
}

int
sys_getgid(void)
{
	return myproc()->gid;
}

int
sys_getegid(void)
{
	return myproc()->egid;
}

int
sys_getuid(void)
{
	return myproc()->uid;
}

int
sys_geteuid(void)
{
	return myproc()->euid;
}

int
sys_fork(void)
{
	return fork();
}

void
sys_exit(void)
{
	int status;
	if(argint(0, &status) < 0) { // Get exit status from first argument
		errno=EPERM;
		return;
	}
	exit(status);
}

int sys_waitpid(void){
	int *status;
	if(argptr(0, (void*)&status, sizeof(*status)) < 0) {
		errno = EFAULT;
		return -1;
	}
	return (waitpid(-1, status, 0)); // Wait for any child
}

int sys_getrusage(void){
	return 0;
}

int
sys_wait4(void)
{
	int pid;
	int *status;
	int options;

	if(argint(0, &pid) < 0)
		return -1;
	if(argptr(1, (void*)&status, sizeof(*status)) < 0)
		return -1;
	if(argint(2, &options) < 0)
		return -1;

	return waitpid(pid, status, options);
}

int
sys_kill(void)
{
	int pid, signo;

	if(argint(0, &pid) < 0)
		return -1;

	if(argint(1, &signo) < 0)
		signo = SIGTERM;

	if(signo < 1 || signo >= NSIG)
		return -1;

	return kill(pid, signo);
}

int
sys_getpid(void)
{
	return myproc()->pid;
}

int
sys_getppid(void)
{
	return myproc()->parent->pid;
}

int
sys_sbrk(void)
{
	int addr;
	int n;

	if(argint(0, &n) < 0) {
		errno=EPERM;
		return -1;
	}
	addr = myproc()->sz;
	if(growproc(n) < 0) {
		errno=5;
		return -1;
	}
	return addr;
}

int
sys_sleep(void)
{
	int n;
	unsigned int ticks0;

	if(argint(0, &n) < 0) {
		errno=EPERM;
		return -1;
	}
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

void notim(){
	printk("syscall %d: Not Implemented.\n", myproc()->tf->eax);
	errno=ENOSYS;
}

int sys_syscall(void){
	notim();
	return -1;
}

/*
 * I would call sys_open, but I don't know how.
 */

int
sys_creat(void)
{
	char *path;
	int mode;
	int fd;
	struct inode *ip;
	struct file *f;

	if(argstr(0,&path)<0 || argint(1,&mode)<0){
		errno=EPERM;
		return -1;
	}

	begin_op();

	/* forced create+truncate */
	ip=create(path,S_IFREG|mode,0,0);
	if(ip==0){
		end_op();
		errno=ENOENT;
		return -1;
	}

	itrunc(ip);

	if((f=filealloc())==0 || (fd=fdalloc(f))<0){
		if(f){
			ip->lmtime=epoch_mktime();
			fileclose(f);
		}
		iunlockput(ip);
		end_op();
		return -1;
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

int sys_break(void){
	notim();
	return -1;
}
int sys_stat(void){
	notim();
	return -1;
}
int sys_mount(void){
	notim();
	return -1;
}
int sys_umount(void){
	notim();
	return -1;
}
int sys_ptrace(void){
	notim();
	return -1;
}
int sys_alarm(void){
	int ticks;

	if(argint(0, &ticks) < 0)
		return -1;

	if(ticks < 0)
		return -1;

	struct proc *p = myproc();
	int old = p->alarminterval;

	p->alarmticks = ticks;
	p->alarminterval = ticks;

	return old;
}

int sys_pause(void){
	notim();
	return -1;
}

int sys_access(void){
	char *path;
	int mode;
	struct inode *ip;
	int perm;
	int uid = myproc()->uid;
	int gid = myproc()->gid;

	if (argstr(0, &path) < 0 || argint(1, &mode) < 0) {
		errno=EPERM;
		return -1;
	}

	mode &= 0007;

	begin_op();
	ip = namei(path);
	if (!ip) {
		end_op();
		return -1;
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
	return -13;
}

int sys_nice(void){
	notim();
	return -1;
}
int sys_ftime(void){
	notim();
	return -1;
}
int sys_rename(void){
	notim();
	return -1;
}
int sys_rmdir(void){
	struct inode *ip, *dp;
	struct dirent de;
	char name[DIRSIZ], *path;
	unsigned int off;

	if(argstr(0, &path) < 0){
		errno=EPERM;
		return -1;
	}

	begin_op();
	if((dp = nameiparent(path, name)) == 0){
		end_op();
		errno=ENOENT;
		return -1;
	}

	ilock(dp);

	if (myproc()->uid != dp->uid) {
		iunlockput(dp);
		end_op();
		errno=EPERM;
		return -1;
	}

	if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0) {
		iunlockput(dp);
		end_op();
		errno=EPERM;
		return -1;
	}

	if((ip = dirlookup(dp, name, &off)) == 0) {
		iunlockput(dp);
		end_op();
		errno=ENOENT;
		return -1;
	}
	ilock(ip);

	if(!S_ISDIR(ip->mode)) {
		iunlockput(ip);
		iunlockput(dp);
		end_op();
		errno=ENOTDIR;
		return -1;
	}

	if(!isdirempty(ip)) { // ensure the directory is empty
		iunlockput(ip);
		iunlockput(dp);
		end_op();
		errno=ENOTEMPTY;
		return -1;
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
int
sys_brk(void)
{
	int addr;
	int old;
	int delta;

	if(argint(0, &addr) < 0) {
		errno = EPERM;
		return -1;
	}

	old = myproc()->sz;

	if(addr == 0)
		return old;

	if(addr < 0) {
		errno = EINVAL;
		return -1;
	}

	delta = addr - old;

	if(delta != 0) {
		if(growproc(delta) < 0) {
			errno = ENOMEM;
			return -1;
		}
	}

	return addr;
}

int sys_signal(void){
	int signum;
	unsigned int handler;
	struct proc *p = myproc();

	if(argint(0, &signum) < 0 || argint(1, (int*)&handler) < 0)
		return -1;

	if(signum < 1 || signum >= NSIG || signum == SIGKILL)
		return -1;  // Can't catch SIGKILL

	unsigned int old = p->sighandlers[signum];
	p->sighandlers[signum] = handler;

	return old;
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

	ws->ws_row = 24;
	ws->ws_col = 80;
	ws->ws_xpixel = 0;
	ws->ws_ypixel = 0;

	return 0;
}

/*
 * palceholder for IOCTL to get printf working under MUSL
 */
int
sys_ioctl(void)
{
	struct file *f;
	int req;
	void *arg;

	if(argfd(0, 0, &f) < 0)
		return -1;
	if(argint(1, &req) < 0)
		return -1;
	if(argptr(2, (char**)&arg, sizeof(struct winsize)) < 0)
		return -1;

	switch(req){
		case 19258: // graphics/console
			if(arg){ // graphics
				vgaconsoleinit();
			} else { // console
				vgareturnconsole();
			}
			return 0;
		case 21523: // winsize
			return tty_get_winsize((struct winsize*)arg);
	default:
		return -1;
	}
}
int sys_fcntl(void){
	int fd;
	int cmd;
	unsigned long arg;
	struct file *f;

	if(argfd(0, &fd, &f) < 0)
		return -1;
	if(argint(1, &cmd) < 0)
		return -1;
	if(argint(2, (int*)&arg) < 0)
		return -1;
	switch(cmd){
	case F_GETFL:
		return f->flags;
	case F_SETFL:
		// only allow changing O_APPEND for now
		if(arg & ~O_APPEND)
			return -1;
		f->flags = (f->flags & ~O_APPEND) | (arg & O_APPEND);
		return 0;
	default:
		return -1;
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
int sys_umask(void){
	notim();
	return -1;
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
	int fd;
	struct file *f;

	if(argfd(0, &fd, &f) < 0)
		return -1;
	myproc()->ofile[fd] = 0;
	fileclose(f);

	return 0;
}
int sys_getpgrp(void){
	notim();
	return -1;
}
int sys_setsid(void){
	notim();
	return -1;
}
int sys_sigaction(void){
	notim();
	return -1;
}
int sys_sgetmask(void){
	notim();
	return -1;
}
int sys_ssetmask(void){
	notim();
	return -1;
}

int sys_getpgid(void){
	return myproc()->parent->gid;
}

/*
 * multiple buffer write
 */
int
sys_writev(void)
{
	struct file *f;
	int count;
	unsigned int *vec;
	int i;
	int total;

	if(argfd(0, 0, &f) < 0 || argint(2, &count) < 0)
		return -1;
	if(argptr(1, (char**)&vec, count * sizeof(unsigned int) * 2) < 0)
		return -1;

	total = 0;
	for(i = 0; i < count; i++){
		char *p = (char*)vec[i * 2];
		int n = (int)vec[i * 2 + 1];
		int r = filewrite(f, p, n);
		if(r < 0)
			return -1;
		total += r;
	}
	return total;
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
int
sys_rt_sigprocmask(void)
{
	int how;
	unsigned int *set;
	unsigned int *oldset;
	size_t sigsetsize;

	if(argint(0, &how) < 0)
		return -1;
	if(argptr(1, (void*)&set, sizeof(*set)) < 0)
		set = 0;
	if(argptr(2, (void*)&oldset, sizeof(*oldset)) < 0)
		oldset = 0;
	if(argint(3, (int*)&sigsetsize) < 0)
		sigsetsize = sizeof(unsigned int);

	struct proc *curproc = myproc();

	// return old mask if requested
	if(oldset != 0) {
		*oldset = curproc->sigmask;
	}

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
				return -1;
		}
	}

	return 0;
}

int
sys_rt_sigaction(void)
{
	int signum;
	struct {
		unsigned int sa_handler;
		unsigned int sa_flags;
		unsigned int sa_restorer;
		unsigned int sa_mask;
	} *act;
	unsigned int old;

	if(argint(0, &signum) < 0)
		return -1;
	if(argptr(1, (void*)&act, sizeof(*act)) < 0)
		act = 0;
	if(argint(2, (int*)&old) < 0)
		old = 0;

	if(signum < 1 || signum >= NSIG || signum == SIGKILL)
		return -1;

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
 * build a path from the proc's cwd
 */
int sys_getcwd(void){
	char *buf;
	size_t size;

	if(argptr(0, (void*)&buf, sizeof(*buf)) < 0)
		return -1;
	if(argint(1, (int*)&size) < 0)
		return -1;

	struct proc *curproc = myproc();
	struct inode *cwd = curproc->cwd;
	char path[128];
	int pos = 127;
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

int sys_fcntl64(void){
	return sys_fcntl();
}

/*
 * we aint got no threads
 */
int sys_exit_group(void){
	int status;

	if(argint(0, &status) < 0)
		return -1;

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
		return -1;

	return myproc()->pid;
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
		return -1;

	if((ip = namei(pathname)) == 0)
		return -1;

	ilock(ip);

	memset(&st, 0, sizeof(st));
	stati(ip, &st);
	memset(&stxbuf, 0, sizeof(stxbuf));

	stxbuf.stx_mask = mask;
	stxbuf.stx_blksize = BSIZE;
	stxbuf.stx_nlink = st.st_nlink;
	stxbuf.stx_uid = st.st_uid;
	stxbuf.stx_gid = st.st_gid;
	stxbuf.stx_mode = st.st_mode | S_IFREG;
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
		return -1;

	return 0;
}

int
sys_clock_gettime64(void)
{
	int clkid;
	struct timespec64 *utp;
	uint64_t ns;

	if(argint(0, &clkid) < 0)
		return -1;
	if(argptr(1, (void*)&utp, sizeof(*utp)) < 0)
		return -1;

	ns = ((rdtsc() - tsc_offset) * 1000000000ULL) / tsc_freq_hz;

	if(clkid != CLOCK_MONOTONIC)
		return -1;

	struct timespec64 ts;
	ts.tv_sec = ns / 1000000000ULL;
	ts.tv_nsec = ns % 1000000000ULL;

	if(copyout(myproc()->pgdir, (unsigned int)utp, &ts, sizeof(ts)) < 0)
		return -1;

	return 0;
}

