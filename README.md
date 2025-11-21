# μ86

Issues with micro86:
	- lacks signals
		- don't let the kernel print segfaults. Send the signal to process.
	- kernel does NOT do ENV correctly. It is usermode memory, not kernel.
		- in fact, gut it all. It is abhorrent and dangerous.
		- by proxy, does not have execve
	- userspace/kernelspace are way to close together
	- memory manager is janky, but workable. Don't rewrite
		- and don't mess with multiboot. It works, don't touch it.
	- janky console drivers, make compatible with ANYTHING besides stty/gtty.
		- ECHO is terrible, and whatever I did to get that bash-like arrow keys working is absolutely aborrent.
	- strip old Unix code, it is mangled and terrible.
		- BSD included!
	- just add ttys.
	- implement: sys_waitpid, sys_execve, sys_break, sys_alarm, sys_access, sys_nice, sys_signal, etc.
		- things that aren't doable from Linux will be placeholdered.
	- DIRENT is currently terrible. Make it it's own header and port it properly.
		- Linux is a good example.
	- the filesystem limitations may seem imposing, but aren't the worst. Don't bother, don't feature-creep.
		- don't mess with inode sise. 14 characters is fine for most applciations, at least for now.
	- console buffering does not exist. Huge bottleneck. Rewrite everything there.
		- the fact that cprintf and printf are totally different and are all nested within sucks ass and is slow.
		- just gut most of console.c, seriously. oldcgaputc and cgaputc? Neither of them are even that!
	- feature creep. Don't add graphics before signals!
		- ramdisk could be okay to add, but no. Kernelmemfs works.
	- get rid of the usermode compiling, that should be seperate. The kernel is the kernel, everything else is unknown to us.
		- Pou supreme
	- get rid of the terrible sys/ structure, do things like bsd or linux
		- this includes drivers/ at the root level. Not only that, but make include/ more structured.
	- jesus fuck ulib.c needs to go NOW.
		- get programs compiling for this operating system outisde of it's initial build alongside the kernel (musl/gcc?)
	- begone with fuckass mkfs!
	- gut most of the Makefile. ULIB shall not be compiled with the kernel.
	- Name sounds bad! redo.
		- I don't have a good name as of now... hmm, wait for inspiration to strike. FreeNIX sounds like penis
	- clean up everything. Good god main.c hurts.
		- mixes BSD code unnecessarily to get CPU name. Screw that.
	- gut sysfile's devctl. It is a nonsense solution to a problem that deserves better treatment.
		- in fact, clean up most syscalls in general. They should not be so large.
	- code uses inconsistent tab spaces. A pet peeve of mine. Either make them all normal \t, or don't bother.
		- \t saves on space, looks cleaner, and can help improve readability.
Don't feature creep. Do what linux did by 0.01

