# μ86
micro86 is a versatile, console-only embedded Unix-like kernel.

# Issues with micro86:

	- kernel does NOT do ENV correctly. It is usermode memory, not kernel.
		- by proxy, does not have execve
	- janky console drivers, make compatible with ANYTHING besides stty/gtty.
	- just add ttys.
	- implement: sys_waitpid, sys_execve, sys_break, sys_alarm, sys_access, sys_nice, sys_signal, etc.
		- things that aren't doable from Linux will be placeholdered.
Don't feature creep. Do what linux did by 0.01
