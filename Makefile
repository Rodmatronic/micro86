S=sys
C=cmd
M=misc
I=include

OBJS = \
	$S/bio.o\
	$S/console.o\
	$S/exec.o\
	$S/file.o\
	$S/font8x16.o\
	$S/fs.o\
	$S/ide.o\
	$S/ioapic.o\
	$S/kalloc.o\
	$S/kbd.o\
	$S/lapic.o\
	$S/log.o\
	$S/main.o\
	$S/mp.o\
	$S/mouse.o\
	$S/picirq.o\
	$S/pipe.o\
	$S/proc.o\
	$S/sleeplock.o\
	$S/spinlock.o\
	$S/string.o\
	$S/swtch.o\
	$S/syscall.o\
	$S/sysfile.o\
	$S/sysproc.o\
	$S/trapasm.o\
	$S/trap.o\
	$S/time.o\
	$S/uart.o\
	$S/vectors.o\
	$S/vm.o\
	$S/vga.o\

# Cross-compiling (e.g., on Mac OS X)
# TOOLPREFIX = i386-jos-elf

# Using native tools (e.g., on X86 Linux)
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'i386-jos-elf-'; \
	elif objdump -i 2>&1 | grep 'elf32-i386' >/dev/null 2>&1; \
	then echo ''; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
	echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2; \
	echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than 'i386-jos-elf-', set your TOOLPREFIX" 1>&2; \
	echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

# If the makefile can't find QEMU, specify its path here
# QEMU = qemu-system-i386

# Try to infer the correct QEMU
ifndef QEMU
QEMU = $(shell if which qemu > /dev/null; \
	then echo qemu; exit; \
	elif which qemu-system-i386 > /dev/null; \
	then echo qemu-system-i386; exit; \
	elif which qemu-system-x86_64 > /dev/null; \
	then echo qemu-system-x86_64; exit; \
	else \
	qemu=/Applications/Q.app/Contents/MacOS/i386-softmmu.app/Contents/MacOS/i386-softmmu; \
	if test -x $$qemu; then echo $$qemu; exit; fi; fi; \
	echo "***" 1>&2; \
	echo "*** Error: Couldn't find a working QEMU executable." 1>&2; \
	echo "*** Is the directory containing the qemu binary in your PATH" 1>&2; \
	echo "*** or have you tried setting the QEMU variable in Makefile?" 1>&2; \
	echo "***" 1>&2; exit 1)
endif

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump
CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -Oz -Wall -MD -m32 -fno-omit-frame-pointer -Wno-infinite-recursion -Wno-implicit-int -Wno-char-subscripts -Wno-implicit-function-declaration -Wno-dangling-else -Wno-int-conversion -Wno-missing-braces

CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
ASFLAGS = -m32 -gdwarf-2 -Wa,-divide
# FreeBSD ld wants ``elf_i386_fbsd''
LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null | head -n 1)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

xv6.img: $S/bootblock $S/kernel
	dd if=/dev/zero of=xv6.img count=10000
	dd if=$S/bootblock of=xv6.img conv=notrunc
	dd if=$S/kernel of=xv6.img seek=1 conv=notrunc

xv6memfs.img: $S/bootblock $S/kernelmemfs
	dd if=/dev/zero of=xv6memfs.img count=10000
	dd if=$S/bootblock of=xv6memfs.img conv=notrunc
	dd if=$S/kernelmemfs of=xv6memfs.img seek=1 conv=notrunc

$S/bootblock: $S/bootasm.S $S/bootmain.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c $S/bootmain.c -o $S/bootmain.o
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c $S/bootasm.S -o $S/bootasm.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o $S/bootblock.o $S/bootasm.o $S/bootmain.o
	$(OBJDUMP) -S $S/bootblock.o > $S/bootblock.asm
	$(OBJCOPY) -S -O binary -j .text $S/bootblock.o $S/bootblock
	$S/sign.pl $S/bootblock

$S/entryother: $S/entryother.S
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c $S/entryother.S -o $S/entryother.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o $S/bootblockother.o $S/entryother.o
	$(OBJCOPY) -S -O binary -j .text $S/bootblockother.o $S/entryother
	$(OBJDUMP) -S $S/bootblockother.o > $S/entryother.asm

$S/initcode: $S/initcode.S
	$(CC) $(CFLAGS) -nostdinc -I. -c $S/initcode.S -o $S/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $S/initcode.out $S/initcode.o
	$(OBJCOPY) -S -O binary $S/initcode.out $S/initcode
	$(OBJDUMP) -S $S/initcode.o > $S/initcode.asm

$S/kernel: $(OBJS) $S/entry.o $S/entryother $S/initcode $S/kernel.ld
	$(LD) $(LDFLAGS) -T $S/kernel.ld -o $S/kernel $S/entry.o $(OBJS) -b binary $S/initcode $S/entryother
	$(OBJDUMP) -S $S/kernel > $S/kernel.asm
	$(OBJDUMP) -t $S/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $S/kernel.sym

# kernelmemfs is a copy of kernel that maintains the
# disk image in memory instead of writing to a disk.
# This is not so useful for testing persistent storage or
# exploring disk buffering implementations, but it is
# great for testing the kernel on real hardware without
# needing a scratch disk.
MEMFSOBJS = $(filter-out $S/ide.o,$(OBJS)) $S/memide.o
$S/kernelmemfs: $(MEMFSOBJS) $S/entry.o $S/entryother $S/initcode $S/kernel.ld $S/fs.img
	$(LD) $(LDFLAGS) -T $S/kernel.ld -o $S/kernelmemfs $S/entry.o  $(MEMFSOBJS) -b binary $S/initcode $S/entryother $S/fs.img
	$(OBJDUMP) -S $S/kernelmemfs > $S/kernelmemfs.asm
	$(OBJDUMP) -t $S/kernelmemfs | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $S/kernelmemfs.sym

tags: $(OBJS) $S/entryother.S $S/_init
	etags *.S *.c

$S/vectors.S: $S/vectors.pl
	$S/vectors.pl > $S/vectors.S

ULIB = $S/ulib.o $S/usys.o $S/printf.o $S/umalloc.o $S/udate.o $S/errno.o $C/font8x16.o $S/ugraphics.o $S/ucrypt.o

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$C/_forktest: $C/forktest.o $(ULIB)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $C/_forktest $C/forktest.o $I/ulib.o $S/usys.o
	$(OBJDUMP) -S $C/_forktest > $C/forktest.asm

$S/mkfs: $S/mkfs.c $S/../include/fs.h
	gcc -o $S/mkfs $S/mkfs.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

UPROGS=\
        $C/_banner\
        $C/_basename\
	$C/_cat\
	$C/_cmp\
	$C/_cp\
	$C/_cron\
	$C/_date\
	$C/_debugger\
	$C/_echo\
	$C/_ed\
	$C/_find\
	$C/_grep\
	$C/_init\
	$C/_kill\
	$C/_ln\
	$C/_login\
	$C/_ls\
	$C/_man\
	$C/_mkdir\
	$C/_mknod\
	$C/_more\
	$C/_mv\
	$C/_pwd\
	$C/_usertests\
	$C/_rm\
	$C/_rmdir\
	$C/_sh\
	$C/_desktop\
	$C/_hellogui\
	$C/_sleep\
	$C/_stressfs\
	$C/_touch\
	$C/_uname\
	$C/_wc\
	$C/_yes\
	$C/_paint2\
	$C/_hexdump\

$S/fs.img: $S/mkfs $M/README $(UPROGS)
	$S/mkfs $S/fs.img $M/etc/rc $M/etc/rc.local $M/etc/passwd $M/etc/group $M/etc/motd $M/changelog $M/cd.1 $M/COPYRIGHT $(UPROGS)

-include *.d

clean: 
	rm -f $S/*.tex $S/*.dvi $S/*.idx $S/*.aux $S/*.log $S/*.ind $S/*.ilg \
	$S/*.o $S/*.d $S/*.asm $S/*.sym $C/*.tex $C/*.dvi $C/*.idx $C/*.aux $C/*.log $C/*.ind $C/*.ilg \
	$C/*.o $C/*.d $C/*.asm $C/*.sym $S/vectors.S $S/bootblock $S/entryother \
	$S/initcode $S/initcode.out $S/kernel xv6.img $S/fs.img $S/kernelmemfs \
	$S/xv6memfs.img $S/mkfs .gdbinit \
	$(UPROGS)

# make a printout
FILES = $(shell grep -v '^\#' runoff.list)
PRINT = runoff.list runoff.spec README toc.hdr toc.ftr $(FILES)

xv6.pdf: $(PRINT)
	./runoff
	ls -l xv6.pdf

print: xv6.pdf

# run in emulators

bochs : fs.img xv6.img
	if [ ! -e .bochsrc ]; then ln -s dot-bochsrc .bochsrc; fi
	bochs -q

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 2
endif
QEMUOPTS = -accel tcg -drive file=$S/fs.img,index=1,media=disk,format=raw -drive file=xv6.img,index=0,media=disk,format=raw -m 512 $(QEMUEXTRA)

qemu: $S/fs.img xv6.img
	$(QEMU) -serial mon:stdio $(QEMUOPTS)

qemu-memfs: xv6memfs.img
	$(QEMU) -drive file=xv6memfs.img,index=0,media=disk,format=raw -smp $(CPUS) -m 256

qemu-nox: fs.img xv6.img
	$(QEMU) -nographic $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

qemu-gdb: fs.img xv6.img .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -serial mon:stdio $(QEMUOPTS) -S $(QEMUGDB)

qemu-nox-gdb: fs.img xv6.img .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -nographic $(QEMUOPTS) -S $(QEMUGDB)

# CUT HERE
# prepare dist for students
# after running make dist, probably want to
# rename it to rev0 or rev1 or so on and then
# check in that version.

EXTRA=\
	mkfs.c ulib.c ../include/user.h cat.c echo.c grep.c kill.c udate.c\
	ln.c ls.c mkdir.c rm.c stressfs.c usertests.c wc.c\
	printf.c umalloc.c\
	README dot-bochsrc *.pl toc.* runoff runoff1 runoff.list\
	.gdbinit.tmpl gdbutil\

dist:
	rm -rf dist
	mkdir dist
	for i in $(FILES); \
	do \
		grep -v PAGEBREAK $$i >dist/$$i; \
	done
	sed '/CUT HERE/,$$d' Makefile >dist/Makefile
	echo >dist/runoff.spec
	cp $(EXTRA) dist

dist-test:
	rm -rf dist
	make dist
	rm -rf dist-test
	mkdir dist-test
	cp dist/* dist-test
	cd dist-test; $(MAKE) print
	cd dist-test; $(MAKE) bochs || true
	cd dist-test; $(MAKE) qemu

# update this rule (change rev#) when it is time to
# make a new revision.
tar:
	rm -rf /tmp/xv6
	mkdir -p /tmp/xv6
	cp dist/* dist/.gdbinit.tmpl /tmp/xv6
	(cd /tmp; tar cf - xv6) | gzip >xv6-rev10.tar.gz  # the next one will be 10 (9/17)

.PHONY: dist-test dist
