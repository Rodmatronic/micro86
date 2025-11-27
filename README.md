# μ86
micro86 is a versatile, console-only embedded Unix-like kernel.

# Issues with micro86:

	- lacks signals
		- don't let the kernel print segfaults. Send the signal to process.
	- kernel does NOT do ENV correctly. It is usermode memory, not kernel.
		- by proxy, does not have execve
	- janky console drivers, make compatible with ANYTHING besides stty/gtty.
		- ECHO is terrible, and whatever I did to get that bash-like arrow keys working is absolutely aborrent.
	- strip old Unix code, it is mangled and terrible.
		- BSD included!
	- just add ttys.
	- implement: sys_waitpid, sys_execve, sys_break, sys_alarm, sys_access, sys_nice, sys_signal, etc.
		- things that aren't doable from Linux will be placeholdered.
	- the filesystem limitations may seem imposing, but aren't the worst. Don't bother, don't feature-creep.
		- don't mess with inode sise. 14 characters is fine for most applciations, at least for now.
	- get rid of the terrible sys/ structure, do things like bsd or linux
		- this includes drivers/ at the root level. Not only that, but make include/ more structured.
	- begone with fuckass mkfs!
	- gut most of the Makefile. ULIB shall not be compiled with the kernel.
	- clean up everything. Good god main.c hurts.
	- code uses inconsistent tab spaces. A pet peeve of mine. Either make them all normal \t, or don't bother.
		- \t saves on space, looks cleaner, and can help improve readability.
Don't feature creep. Do what linux did by 0.01

