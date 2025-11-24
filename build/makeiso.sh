#!/bin/sh

set -e

ISO_NAME="microunix.iso"
WORKDIR="isotree"

rm -rf "$WORKDIR" "$ISO_NAME"
mkdir -p "$WORKDIR/boot/grub"

cat > "$WORKDIR/boot/grub/grub.cfg" <<EOF
set timeout=0

menuentry "micro86" {
	echo -n 'Loading...'
	multiboot2 /miunix
	echo 'Done.'
	boot
}
EOF

cp sys/kernelmemfs "$WORKDIR/miunix"
grub-mkrescue -o "$ISO_NAME" "$WORKDIR"
echo "ISO created: $ISO_NAME"
