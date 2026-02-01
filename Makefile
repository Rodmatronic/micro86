D=drivers
S=kernel
I=include

TOOLPREFIX ?= i386-elf-
QEMU ?= qemu-system-i386
CC := $(TOOLPREFIX)gcc
CPP := cc
AS := $(TOOLPREFIX)gas
LD := $(TOOLPREFIX)ld
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump

CFLAGS += -MMD -MP -fno-pic -static -fno-builtin -Oz -Wall -MD -m32 -Wa,--noexecstack -Iinclude -Werror
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

ASFLAGS += -m32 -gdwarf-2 -Wa,--noexecstack -Iinclude -DASM_FILE=1
LDFLAGS += -m elf_i386 
QEMUOPTS += -accel tcg -cdrom microunix.iso -boot d -drive file=$S/fs.img,index=1,media=disk,format=raw

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
	$S/signal.o\
	$S/sleeplock.o\
	$S/spinlock.o\
	$S/string.o\
	$S/swtch.o\
	$S/sys/syscall.o\
	$S/sys/sys.o\
	$S/timer.o\
	$S/boot/trapasm.o\
	$S/trap.o\
	$S/tsc.o\
	$S/time.o\
	$S/type64.o\
	$S/pl/vectors.o\
	$S/vm.o\
	$D/char/console.o\
	$D/char/mem.o\
	$D/char/random.o\
	$D/char/tty_aux.o\
	$D/input/keyboard/kbd.o\
	$D/ide/ide.o\
	$D/serial/uart.o\

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

include $(wildcard kernel/*.d)
include $(wildcard drivers/*.d)

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
	$(CPP) -o $S/mkfs/mkfs $S/mkfs/mkfs.c

$S/fs.img: $S/mkfs/mkfs
	build/build.sh
	$S/mkfs/mkfs $S/fs.img root

-include *.d

clean:
	find . -type f \( -name '*.o' -o -name '*.asm' -o -name '*.sym' -o -name '*.d' \) -delete
	rm -rf $S/pl/vectors.S $S/boot/entryother \
	$S/initcode $S/initcode.out $S/miunix xv6.img $S/fs.img $S/kernelmemfs \
	xv6memfs.img $S/mkfs/mkfs .gdbinit microunix.iso
	rm -rf isotree/

iso: $S/fs.img xv6.img
	./build/makeiso.sh

qemu: iso
	$(QEMU) -serial mon:stdio $(QEMUOPTS)

qemu-nox: iso
	$(QEMU) -display curses $(QEMUOPTS)
