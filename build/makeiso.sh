#!/bin/sh

set -e

ISO_NAME="frunix.iso"
WORKDIR="isotree"

rm -rf "$WORKDIR" "$ISO_NAME"
mkdir -p "$WORKDIR/boot/grub"

cat > "$WORKDIR/boot/grub/grub.cfg" <<EOF
set timeout=0
set default=0

menuentry "FreeNIX" {
	echo 'Starting FreeNIX...'
	multiboot /frunix
	echo 'Done.'
	boot
}
EOF

cp sys/kernelmemfs "$WORKDIR/frunix"
grub-mkrescue -o "$ISO_NAME" "$WORKDIR"
echo "ISO created: $ISO_NAME"
