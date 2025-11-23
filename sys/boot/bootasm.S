#include <asm.h>

/* we don't boot from the kernel directly, we multiboot through grub */

.code16
.global _start

/* BIOS teletype: AH=0x0E, AL=char */

_start:
	movw	$msg, %si
	call	print
	cli
	hlt

print:
	lodsb
	cmpb	$0, %al
	je	done
	movb	$0x0E, %ah
	int	$0x10
	call	print
	ret

done:
	ret

msg:
	.ascii	"\nPlease use a bootloader!\r\n"
	.byte	0
