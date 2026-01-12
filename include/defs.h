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
struct rtcdate;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;
extern int tsc_calibrated;
extern unsigned int PHYSTOP;

#define IRQ_IDE_SECONDARY 15

extern unsigned long startup_time;
extern unsigned long kernel_time;
extern uint16_t current_line;

// common functions, some from early Linux
#define suser() (myproc()->euid == 0)


// bio.c
void            binit(void);
struct buf*     bread(unsigned int, unsigned int);
void            brelse(struct buf*);
void            bwrite(struct buf*);
void		sync();

// console.c
#define		printk(fmt, ...) \
			_printf((char *)__func__, fmt, ##__VA_ARGS__)
#define		debug(fmt, ...) \
			uart_debug=1; \
			_printf((char *)__func__, fmt, ##__VA_ARGS__);\
			uart_debug=0;

extern int	current_color;
extern int	x, y;
void            consoleinit(void);
void		consputc(int);
void		_printf(char *func, char *fmt, ...);
void            consoleintr(int(*)(void));
int		consoleread(struct inode *ip, char *dst, int n, uint32_t off);
int		consolewrite(struct inode *ip, char *buf, int n, uint32_t off);
int		sprintf(char *buf, const char *fmt, ...);
void		vkprintf(const char *fmt, va_list ap);

// exec.c
int	      exec(char*, char**);

// file.c
struct file*    filealloc(void);
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit(void);
int             fileread(struct file*, char*, int n);
int             filestat(struct file*, struct stat*);
int             filewrite(struct file*, char*, int n);
void		devinit(void);

// fs.c
uint32_t 	bmap(struct inode *ip, uint32_t bn);
int             dirlink(struct inode*, char*, unsigned int);
struct inode*   dirlookup(struct inode*, char*, unsigned int*);
struct inode*   ialloc(unsigned int, short);
struct inode*   idup(struct inode*);
void            iinit(int dev);
void            ilock(struct inode*);
void            iput(struct inode*);
void            iunlock(struct inode*);
void            iunlockput(struct inode*);
void            iupdate(struct inode*);
int             namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);
int             readi(struct inode*, char*, unsigned int, unsigned int);
void            readsb(int dev, struct superblock *sb);
void            stati(struct inode*, struct stat*);
int             writei(struct inode*, char*, unsigned int, unsigned int);
void		itrunc(struct inode *ip);

// ide.c
void            ideinit(void);
void            ideintr(void);
void            iderw(struct buf*);

// ioapic.c
void            ioapicenable(int irq, int cpu);
extern uchar    ioapicid;
void            ioapicinit(void);

// kalloc.c
char*           kalloc(void);
void            kfree(char*);
void            kinit1(void*, void*);
void            kinit2(void*, void*);

// kbd.c
extern int	mouse_enable;
void            kbdintr(void);
int		kgetchar(void);

// lapic.c
void            cmostime(struct rtcdate *r);
int             lapicid(void);
extern volatile unsigned int*    lapic;
void            lapiceoi(void);
void            lapicinit(void);
void            lapicstartap(uchar, unsigned int);
void            microdelay(int);

// log.c
void            initlog(int dev);
void            log_write(struct buf*);
void            begin_op();
void            end_op();

// main.c
int		reboot(int);

// mouse.c
void		mouseinit(void);

typedef struct {
    uint8_t left_button: 1;
    uint8_t right_button: 1;
    uint8_t middle_button: 1;
    uint8_t always_1: 1;
    uint8_t x_sign: 1;
    uint8_t y_sign: 1;
    uint8_t x_overflow: 1;
    uint8_t y_overflow: 1;
} MOUSE_STATUS;

extern int g_mouse_x_pos, g_mouse_y_pos, left_button, right_button, middle_button;

// multiboot.c
extern char *	cmdline;
void		mbootinit(unsigned long);

// mp.c
extern int      ismp;
void            mpinit(void);

// panic.c
void            panic(char *fmt, ...) __attribute__((noreturn));
extern int	panicked;

// picirq.c
void            picenable(int);
void            picinit(void);

// pipe.c
int             pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, char*, int);
int             pipewrite(struct pipe*, char*, int);

// proc.c
int             cpunum(void);
void            exit(int);
struct proc*	findproc(int, struct proc *parent);
int             fork(void);
int             growproc(int);
int             kill(int, int);
struct cpu*     mycpu(void);
struct proc*    myproc();
void		pause(void);
void            pinit(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            setproc(struct proc*);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             waitpid(int pid, int *status, int options);
void            wakeup(void*);
void            yield(void);

// signal.c
void		dosignal(void);

// swtch.S
void            swtch(struct context**, struct context*);

// spinlock.c
void            acquire(struct spinlock*);
void            getcallerpcs(void*, unsigned int*);
int             holding(struct spinlock*);
void            initlock(struct spinlock*, char*);
void            release(struct spinlock*);
void            pushcli(void);
void            popcli(void);

// sleeplock.c
void            acquiresleep(struct sleeplock*);
void            releasesleep(struct sleeplock*);
int             holdingsleep(struct sleeplock*);
void            initsleeplock(struct sleeplock*, char*);

// string.c
int             memcmp(const void*, const void*, unsigned int);
void*           memmove(void*, const void*, unsigned int);
void*           memset(void*, int, unsigned int);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, unsigned int);
char*           strncpy(char*, const char*, int);

// sys.c
void		ctrl_alt_del(void);

// syscall.c
int             argint(int, int*);
int             argptr(int, char**, int);
int             argstr(int, char**);
int             fetchint(unsigned int, int*);
int             fetchstr(unsigned int, char**);
void            syscall(void);

// time.c
uint8_t		cmos_read(uint8_t);
unsigned int	epoch_mktime(void);
void 		set_kernel_time(unsigned long);

// timer.c
void            timerinit(void);

// trap.c
void            idtinit(void);
extern unsigned int     ticks;
void            tvinit(void);
extern struct spinlock tickslock;

// tsc.c
extern uint64_t tsc_freq_hz;
extern uint64_t tsc_offset;
extern uint64_t tsc_realtime;

uint64_t	rdtsc(void);
void		tscinit(void);
uint64_t	tsc_to_us(uint64_t);
uint64_t	tsc_to_ns(uint64_t);

// uart.c
extern int uart_debug;
void            uartinit(void);
void            uartintr(void);
void            uartputc(int);

// vgagraphics.c
void		graphical_putc(uint16_t, uint16_t, char, uint8_t);
void		putpixel(uint16_t, uint16_t, uint8_t);
void		gvga_clear(void);
void		gvga_init(void);
void		gvga_retcons(void);
void		gvga_scroll(void);

// vgatext.c
void		vgatextpalette(void);

// vm.c
void            seginit(void);
void            kvmalloc(void);
pde_t*          setupkvm(void);
char*           uva2ka(pde_t*, char*);
int             allocuvm(pde_t*, unsigned int, unsigned int);
int             deallocuvm(pde_t*, unsigned int, unsigned int);
void            freevm(pde_t*);
void            inituvm(pde_t*, char*, unsigned int);
int             loaduvm(pde_t*, char*, struct inode*, unsigned int, unsigned int);
pde_t*          copyuvm(pde_t*, unsigned int);
void            switchuvm(struct proc*);
void            switchkvm(void);
int             copyout(pde_t*, unsigned int, void*, unsigned int);
void            clearpteu(pde_t *pgdir, char *uva);
int		mappages(pde_t *pgdir, void *va, unsigned int size, unsigned int pa, int perm);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#endif
