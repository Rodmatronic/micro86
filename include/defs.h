#ifndef _DEFS_H
#define _DEFS_H

#include <stdarg.h>
#include <param.h>

struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct tty;
struct rtcdate;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;
extern int tsc_calibrated;
extern uint32_t PHYSTOP;

#define IRQ_IDE_SECONDARY 15

extern uint16_t current_line;

// common functions, some from early Linux
#define suser() (myproc()->euid == 0)

// bio.c
void		buffer_init(void);
struct buf*     buffer_read(uint32_t, uint32_t);
void		buffer_release(struct buf*);
void		buffer_write(struct buf*);
void		sync();

// console.c
#define		printk(fmt, ...) \
			_printk((char *)__func__, fmt, ##__VA_ARGS__)
#define		debug(fmt, ...) \
			uart_debug=1; \
			_printk((char *)__func__, fmt, ##__VA_ARGS__);\
			uart_debug=0

void		color_change(int);
extern int	current_color;
void		console_init(void);
void		console_putc(int);
void		_printk(const char *func, const char *fmt, ...);
void		printf(const char *format, ...);
void		vprintf(const char *format, va_list args);
void		console_interrupt(int(*)(void));
int		sprintf(char *buf, const char *fmt, ...);

// exec.c
int		exec(char*, char**);

// file.c
struct file*    file_alloc(void);
void		file_close(struct file*);
struct file*    file_dup(struct file*);
void		file_init(void);
int		file_read(struct file*, char*, int n);
int		file_stat(struct file*, struct stat*);
int		file_write(struct file*, char*, int n);

// fs.c
uint32_t 	bmap(struct inode *ip, uint32_t bn);
int		dirlink(struct inode*, char*, uint32_t);
struct inode*   dirlookup(struct inode*, char*, uint32_t*);
struct inode*   ialloc(uint32_t, short);
struct inode*   idup(struct inode*);
void		iinit(int dev);
void		ilock(struct inode*);
void		iput(struct inode*);
void		iunlock(struct inode*);
void		iunlockput(struct inode*);
void		iupdate(struct inode*);
int		namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);
int		readi(struct inode*, char*, uint32_t, uint32_t);
void		readsb(int dev, struct superblock *sb);
void		stati(struct inode*, struct stat*);
int		writei(struct inode*, char*, uint32_t, uint32_t);
void		itrunc(struct inode *ip);

// ide.c
void		ide_init(void);
void		ide_interrupt(void);
void		ide_dirty_write(struct buf*);

// kalloc.c
char*           kalloc(void);
void		kfree(char*);
void		kinit1(void*, void*);
void		kinit2(void*, void*);

// kbd.c
extern int	mouse_enable;
void		keyboard_init(void);
void		kbd_interrupt(void);
int		kget_char(void);

// lapic.c
void		cmostime(struct rtcdate *r);
void		microdelay(int);

// log.c
void		init_log(int dev);
void		log_write(struct buf*);
void		begin_op();
void		end_op();

// multiboot.c
extern char *	cmdline;
void		multiboot_init(unsigned long);

// mp.c
extern int      ismp;
void		mpinit(void);

// panic.c
void		panic(char *fmt, ...) __attribute__((noreturn));
extern int	panicked;

// picirq.c
void		pic_enable(int);
void		pic_init(void);

// pipe.c
int		pipealloc(struct file**, struct file**);
void		pipeclose(struct pipe*, int);
int		piperead(struct pipe*, char*, int);
int		pipewrite(struct pipe*, char*, int);

// proc.c
int		cpunum(void);
void		exit(int);
struct proc*	find_proc(int, struct proc *parent);
int		fork(void);
int		grow_proc(int);
int		kill(int, int);
struct cpu*     mycpu(void);
struct proc*    myproc(void);
void		pause(void);
void		process_init(void);
void		scheduler(void) __attribute__((noreturn));
void		sched(void);
void		setproc(struct proc*);
void		sleep(void*, struct spinlock*);
void		user_init(void);
int		waitpid(int pid, int *status, int options);
void		wakeup(void*);
void		yield(void);

// signal.c
void		dosignal(void);

// swtch.S
void		swtch(struct context**, struct context*);

// spinlock.c
void		acquire(struct spinlock*);
void		getcallerpcs(void*, uint32_t*);
int		holding(struct spinlock*);
void		initlock(struct spinlock*, char*);
void		release(struct spinlock*);
void		pushcli(void);
void		popcli(void);

// sleeplock.c
void		acquiresleep(struct sleeplock*);
void		releasesleep(struct sleeplock*);
int		holdingsleep(struct sleeplock*);
void		initsleeplock(struct sleeplock*, char*);

// string.c
void		itoa(char *, int, int);
int		memcmp(const void*, const void*, uint32_t);
void*           memmove(void*, const void*, uint32_t);
void*           memset(void*, int, uint32_t);
char*           safestrcpy(char*, const char*, int);
int		strlen(const char*);
int		strncmp(const char*, const char*, uint32_t);
char*           strncpy(char*, const char*, int);

// sys.c
void		ctrl_alt_del(void);

// syscall.c
int		argint(int, int*);
int		argptr(int, char**, int);
int		argstr(int, char**);
int		fetchint(uint32_t, int*);
int		fetchstr(uint32_t, char**);
void		syscall(void);

// time.c
uint8_t		cmos_read(uint8_t);
time_t		epoch_mktime(void);
void		time_init(void);
extern time_t	system_time;

// timer.c
void		pit_init(void);

// trap.c
void		idt_init(void);
extern uint32_t     ticks;
void		trap_init();
void		tvinit(void);
extern struct	spinlock tickslock;

// tsc.c
extern uint64_t tsc_freq_hz;
extern uint64_t tsc_offset;
extern uint64_t tsc_realtime;
uint64_t	rdtsc(void);
void		tsc_init(void);
uint64_t	tsc_to_us(uint64_t);
uint64_t	tsc_to_ns(uint64_t);

// tty.c
int		change_tty(int);
void		tty_putc(struct tty *, int);
void		tty_sgr(struct tty *, int);

// uart.c
extern int	uart_debug;
void		uart_init(void);
void		uart_interrupt(void);
void		uart_putc(int);

// vm.c
void		segment_init(void);
void		kvm_alloc(void);
pde_t*          setupkvm(void);
char*           uva2ka(pde_t*, char*);
int		allocuvm(pde_t*, uint32_t, uint32_t);
int		deallocuvm(pde_t*, uint32_t, uint32_t);
void		freevm(pde_t*);
void		inituvm(pde_t*, char*, uint32_t);
int		loaduvm(pde_t*, char*, struct inode*, uint32_t, uint32_t);
pde_t*          copyuvm(pde_t*, uint32_t);
void		switchuvm(struct proc*);
void		switchkvm(void);
int		copyout(pde_t*, uint32_t, void*, uint32_t);
void		clearpteu(pde_t *pgdir, char *uva);
int		mappages(pde_t *pgdir, void *va, uint32_t size, uint32_t pa, int perm);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#endif
