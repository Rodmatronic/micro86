S=sys
C=cmd
M=misc
I=include
L=lib

OBJS = \
	$S/os/multiboot.o \
	$S/os/bio.o\
	$S/driver/console.o\
	$S/os/exec.o\
	$S/os/debugger.o\
	$S/os/file.o\
	$S/graph/font8x8.o\
	$S/os/fs.o\
	$S/driver/ide.o\
	$S/os/ioapic.o\
	$S/os/kalloc.o\
	$S/driver/kbd.o\
	$S/os/lapic.o\
	$S/os/log.o\
	$S/os/main.o\
	$S/os/mp.o\
	$S/driver/mouse.o\
	$S/os/panic.o\
	$S/os/picirq.o\
	$S/os/pipe.o\
	$S/os/proc.o\
	$S/os/sleeplock.o\
	$S/os/spinlock.o\
	$S/os/string.o\
	$S/os/swtch.o\
	$S/sys/syscall.o\
	$S/sys/sysfile.o\
	$S/sys/sysproc.o\
	$S/boot/trapasm.o\
	$S/os/trap.o\
	$S/os/time.o\
	$S/driver/uart.o\
	$S/pl/vectors.o\
	$S/os/vm.o\
	$S/driver/vbe.o\

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

.PHONY: pre
pre:
	@./build/newvers.sh

all: pre xv6.img


CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump
CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -Oz -Wall -MD -m32 -fno-omit-frame-pointer -Wno-infinite-recursion -Wno-implicit-int -Wno-char-subscripts -Wno-implicit-function-declaration -Wno-dangling-else -Wno-int-conversion -Wno-missing-braces -Waggressive-loop-optimizations -Wa,--noexecstack -Iinclude -Werror

CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
ASFLAGS = -m32 -gdwarf-2 -Wa,-divide -Wa,--noexecstack -Iinclude -DASM_FILE=1
# FreeBSD ld wants ``elf_i386_fbsd''
LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null | head -n 1) --no-warn-rwx-segment

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

xv6.img: $S/boot/bootblock $S/frunix
	dd if=/dev/zero of=xv6.img count=10000
	dd if=$S/boot/bootblock of=xv6.img conv=notrunc
	dd if=$S/frunix of=xv6.img seek=1 conv=notrunc

xv6memfs.img: $S/boot/bootblock $S/kernelmemfs
	dd if=/dev/zero of=xv6memfs.img count=10000
	dd if=$S/boot/bootblock of=xv6memfs.img conv=notrunc
	dd if=$S/kernelmemfs of=xv6memfs.img seek=1 conv=notrunc

$S/boot/bootblock: $S/boot/bootasm.S $S/boot/bootmain.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c $S/boot/bootmain.c -o $S/boot/bootmain.o
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c $S/boot/bootasm.S -o $S/boot/bootasm.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o $S/boot/bootblock.o $S/boot/bootasm.o $S/boot/bootmain.o
	$(OBJDUMP) -S $S/boot/bootblock.o > $S/boot/bootblock.asm
	$(OBJCOPY) -S -O binary -j .text $S/boot/bootblock.o $S/boot/bootblock
	$S/pl/sign.pl $S/boot/bootblock

$S/boot/entryother: $S/boot/entryother.S
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c $S/boot/entryother.S -o $S/boot/entryother.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o $S/boot/bootblockother.o $S/boot/entryother.o
	$(OBJCOPY) -S -O binary -j .text $S/boot/bootblockother.o $S/boot/entryother
	$(OBJDUMP) -S $S/boot/bootblockother.o > $S/boot/entryother.asm

$S/os/initcode: $S/os/initcode.S
	$(CC) $(CFLAGS) -nostdinc -I. -c $S/os/initcode.S -o $S/os/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $S/os/initcode.out $S/os/initcode.o
	$(OBJCOPY) -S -O binary $S/os/initcode.out $S/os/initcode
	$(OBJDUMP) -S $S/os/initcode.o > $S/os/initcode.asm

$S/frunix: $(OBJS) $S/boot/entry.o $S/boot/entryother $S/os/initcode $S/boot/kernel.ld
	$(LD) $(LDFLAGS) -T $S/boot/kernel.ld -o $S/frunix $S/boot/entry.o $(OBJS) -b binary $S/os/initcode $S/boot/entryother
	$(OBJDUMP) -S $S/frunix > $S/kernel.asm
	$(OBJDUMP) -t $S/frunix | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $S/kernel.sym

# kernelmemfs is a copy of kernel that maintains the
# disk image in memory instead of writing to a disk.
# This is not so useful for testing persistent storage or
# exploring disk buffering implementations, but it is
# great for testing the kernel on real hardware without
# needing a scratch disk.
MEMFSOBJS = $(filter-out $S/driver/ide.o,$(OBJS)) $S/driver/memide.o
$S/kernelmemfs: $(MEMFSOBJS) $S/boot/entry.o $S/boot/entryother $S/os/initcode $S/boot/kernel.ld $S/fs.img
	$(LD) $(LDFLAGS) -T $S/boot/kernel.ld -o $S/kernelmemfs $S/boot/entry.o  $(MEMFSOBJS) -b binary $S/os/initcode $S/boot/entryother $S/fs.img
	$(OBJDUMP) -S $S/kernelmemfs > $S/kernelmemfs.asm
	$(OBJDUMP) -t $S/kernelmemfs | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $S/kernelmemfs.sym

tags: $(OBJS) $S/boot/entryother.S $S/_init
	etags *.S *.c

$S/pl/vectors.S: $S/pl/vectors.pl
	$S/pl/vectors.pl > $S/pl/vectors.S

ULIB = $L/ulib.o $S/sys/usys.o $L/printf.o $S/os/umalloc.o $L/udate.o $L/errno.o $C/font8x16.o $L/ucrypt.o $L/setmode.o $L/reallocarray.o $L/isctype.o $L/syslog.o $L/string.o

_%: %.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $@ $^
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$C/_forktest: $C/forktest.o $(ULIB)
	# forktest has less library code linked in - needs to be small
	# in order to be able to max out the proc table.
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $C/_forktest $C/forktest.o $I/ulib.o $S/usys.o
	$(OBJDUMP) -S $C/_forktest > $C/forktest.asm

$S/mkfs/mkfs: $S/mkfs/mkfs.c $S/../include/fs.h
	gcc -o $S/mkfs/mkfs $S/mkfs/mkfs.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

UPROGS=\
        $C/_banner\
        $C/_basename\
	$C/_cat\
	$C/_chmod\
	$C/_chown\
	$C/_cmp\
	$C/_cp\
	$C/_cron\
	$C/_date\
	$C/_dd\
	$C/_du\
	$C/_debugger\
	$C/_dirname\
	$C/_echo\
	$C/_ed\
	$C/_env\
	$C/_find\
	$C/_fortune\
	$C/_getty\
	$C/_grep\
	$C/_hexdump\
	$C/_hostname\
	$C/_init\
	$C/_kill\
	$C/_line\
	$C/_ln\
	$C/_login\
	$C/_ls\
	$C/_man\
	$C/_mkdir\
	$C/_mknod\
	$C/_more\
	$C/_mv\
	$C/_nologin\
	$C/_passwd\
	$C/_ps\
	$C/_pwd\
	$C/_usertests\
	$C/_rm\
	$C/_rmdir\
	$C/_sh\
	$C/_su\
	$C/_reboot\
	$C/_sleep\
	$C/_stressfs\
	$C/_touch\
	$C/_uname\
	$C/_wc\
	$C/_which\
	$C/_whoami\
	$C/_yes\

ETC=\
    	$M/lib/fortunes\
	$M/etc/gettytab\
	$M/etc/rc\
	$M/etc/rc.local\
	$M/etc/master.passwd\
	$M/etc/group\
	$M/etc/motd\
	$M/etc/colortest\

$S/fs.img: $S/mkfs/mkfs $(UPROGS)
	build/build.sh
	$S/mkfs/mkfs $S/fs.img $M/cd.1 $(UPROGS) $(ETC)

-include *.d

clean:
	find $S $C $L -type f \( -name '*.o' -o -name '*.asm' -o -name '*.sym' -o -name '*.tex' -o -name '*.dvi' -o -name '*.idx' -o -name '*.aux' -o -name '*.log' -o -name '*.ind' -o -name '*.ilg' -o -name '*.d' \) -delete
	rm -rf $S/pl/vectors.S $S/boot/bootblock $S/boot/entryother \
	$S/os/initcode $S/os/initcode.out $S/frunix xv6.img $S/fs.img $S/kernelmemfs \
	xv6memfs.img $S/mkfs/mkfs .gdbinit frunix.iso include/version.h $(UPROGS)
	rm -r isotree/

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

image: xv6memfs.img
	#awesome

qemu: $S/fs.img xv6.img
	$(QEMU) -serial mon:stdio $(QEMUOPTS)

qemu-memfs: xv6memfs.img
	$(QEMU) -cdrom frunix.iso -smp $(CPUS) -m 128 -serial mon:stdio -accel tcg

qemu-nox: $S/fs.img xv6.img
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
	mkfs/mkfs.c ulib.c ../include/stdio.h cat.c echo.c grep.c kill.c udate.c\
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
