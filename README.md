# Trinix
This is my hobby-operating system that I work on in my free time. Don't expect too much out of it, I only recently got it to run MKSH properly.

Trinix requires at least 128 MB of ram. While it can go lower, you won't be able to run all too many programs

The version number only gets incremented when I add a whole bunch of stuff that is really groundbreaking. Like a ton of new syscalls, or a ton of fundamental fixes to the kernel. This is, of course, based on XV6. But, I have gutted a fair bit of the original project to be a more Linux-like kernel. No userspace comes with this by default, and any programs you wish to run have to be statically linked, and have a small enough entrypoint (0x10000 - 0x15000 works well!).

Anything in root/ will be asssembled into a funky little disk by MKFS. Mind you that regular Linux stuff WILL NOT run. (aside from executables compiled explicitly for this funky kernel)
