#!/bin/sh

set -e

ISO_NAME="frunix.iso"
WORKDIR="isotree"

rm -rf "$WORKDIR" "$ISO_NAME"
mkdir -p "$WORKDIR/boot/grub"

cat > "$WORKDIR/boot/grub/grub.cfg" <<EOF
set timeout=0

menuentry "FreeNIX" {
	echo 'Starting FreeNIX...'
	multiboot2 /frunix
    module2 /fs.img
	echo 'Done.'
	boot
}
EOF

cp sys/frunix "$WORKDIR/frunix"
cp sys/fs.img "$WORKDIR/fs.img"
grub-mkrescue -o "$ISO_NAME" "$WORKDIR"
echo "ISO created: $ISO_NAME"
