D=drivers
S=sys
I=include
L=lib

#kernel, then drivers
OBJS = \
	$S/bio.o\
	$S/exec.o\
	$S/file.o\
	$S/fs.o\
	$S/ioapic.o\
	$S/kalloc.o\
	$S/lapic.o\
	$S/log.o\
	$S/main.o\
	$S/boot/multiboot.o\
	$S/mp.o\
	$S/panic.o\
	$S/picirq.o\
	$S/pipe.o\
	$S/proc.o\
	$S/sleeplock.o\
	$S/spinlock.o\
	$S/string.o\
	$S/swtch.o\
	$S/sys/syscall.o\
	$S/sys/sys.o\
	$S/boot/trapasm.o\
	$S/trap.o\
	$S/time.o\
	$S/pl/vectors.o\
	$S/vm.o\
	$D/console.o\
	$D/kbd.o\
	$D/ide.o\
	$D/uart.o\

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

#all: pre xv6.img

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump
CFLAGS = -fno-pic -static -fno-builtin -Oz -Wall -MD -m32 -Wa,--noexecstack -Iinclude -Werror

CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
ASFLAGS = -m32 -gdwarf-2 -Wa,--noexecstack -Iinclude -DASM_FILE=1
# FreeBSD ld wants ``elf_i386_fbsd''
LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null | head -n 1) --no-warn-rwx-segment

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

xv6.img: $S/miunix
	dd if=/dev/zero of=xv6.img count=10000
	dd if=$S/miunix of=xv6.img seek=1 conv=notrunc

xv6memfs.img: $S/boot/bootblock $S/kernelmemfs
	dd if=/dev/zero of=xv6memfs.img count=10000
	dd if=$S/boot/bootblock of=xv6memfs.img conv=notrunc
	dd if=$S/kernelmemfs of=xv6memfs.img seek=1 conv=notrunc

$S/boot/entryother: $S/boot/entryother.S
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c $S/boot/entryother.S -o $S/boot/entryother.o
	$(OBJCOPY) -S -O binary -j .text $S/boot/entryother.o $S/boot/entryother
	$(OBJDUMP) -S $S/boot/entryother.o > $S/boot/entryother.asm

$S/initcode: $S/initcode.S
	$(CC) $(CFLAGS) -nostdinc -I. -c $S/initcode.S -o $S/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $S/initcode.out $S/initcode.o
	$(OBJCOPY) -S -O binary $S/initcode.out $S/initcode
	$(OBJDUMP) -S $S/initcode.o > $S/initcode.asm

$S/miunix: $(OBJS) $S/boot/entry.o $S/boot/entryother $S/initcode $S/boot/kernel.ld
	$(LD) $(LDFLAGS) -T $S/boot/kernel.ld -o $S/miunix $S/boot/entry.o $(OBJS) -b binary $S/initcode $S/boot/entryother
	$(OBJDUMP) -S $S/miunix > $S/kernel.asm
	$(OBJDUMP) -t $S/miunix | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $S/kernel.sym

# kernelmemfs is a copy of kernel that maintains the
# disk image in memory instead of writing to a disk.
# This is not so useful for testing persistent storage or
# exploring disk buffering implementations, but it is
# great for testing the kernel on real hardware without
# needing a scratch disk.
MEMFSOBJS = $(filter-out $S/driver/ide.o,$(OBJS)) $S/driver/memide.o
$S/kernelmemfs: $(MEMFSOBJS) $S/boot/entry.o $S/boot/entryother $S/initcode $S/boot/kernel.ld $S/fs.img
	$(LD) $(LDFLAGS) -T $S/boot/kernel.ld -o $S/kernelmemfs $S/boot/entry.o  $(MEMFSOBJS) -b binary $S/initcode $S/boot/entryother $S/fs.img
	$(OBJDUMP) -S $S/kernelmemfs > $S/kernelmemfs.asm
	$(OBJDUMP) -t $S/kernelmemfs | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $S/kernelmemfs.sym

tags: $(OBJS) $S/boot/entryother.S $S/_init
	etags *.S *.c

$S/pl/vectors.S: $S/pl/vectors.pl
	$S/pl/vectors.pl > $S/pl/vectors.S

$S/mkfs/mkfs: $S/mkfs/mkfs.c $S/../include/fs.h
	gcc -o $S/mkfs/mkfs $S/mkfs/mkfs.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

$S/fs.img: $S/mkfs/mkfs $(UPROGS)
	build/build.sh
	$S/mkfs/mkfs $S/fs.img

-include *.d

clean:
	find $S $C $L -type f \( -name '*.o' -o -name '*.asm' -o -name '*.sym' -o -name '*.tex' -o -name '*.dvi' -o -name '*.idx' -o -name '*.aux' -o -name '*.log' -o -name '*.ind' -o -name '*.ilg' -o -name '*.d' \) -delete
	rm -rf $S/pl/vectors.S $S/boot/entryother \
	$S/initcode $S/initcode.out $S/miunix xv6.img $S/fs.img $S/kernelmemfs \
	xv6memfs.img $S/mkfs/mkfs .gdbinit microunix.iso $(UPROGS)
	rm -rf isotree/

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
QEMUOPTS = -accel tcg -cdrom microunix.iso -boot d -drive file=sys/fs.img,index=1,media=disk,format=raw$(QEMUEXTRA)

qemu: $S/fs.img xv6.img
	$(QEMU) -serial mon:stdio $(QEMUOPTS)

#qemu-memfs: xv6memfs.img
#	$(QEMU) -cdrom microunix.iso -smp $(CPUS) -m 128 -serial mon:stdio -accel tcg
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
