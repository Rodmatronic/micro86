#!/bin/sh

set -e

ISO_NAME="microunix.iso"
WORKDIR="isotree"
KERNEL="kernel/miunix"
FILE="/miunix"

rm -rf "$WORKDIR" "$ISO_NAME"
mkdir -p "$WORKDIR/boot/grub"

cat > "$WORKDIR/boot/grub/grub.cfg" <<EOF
set timeout=5

menuentry "micro86 at $FILE" {
	echo -n 'Loading...'
	multiboot2 $FILE
	echo 'Done.'
	boot
}
EOF

cp $KERNEL "$WORKDIR/miunix"
grub-mkrescue -o "$ISO_NAME" "$WORKDIR" \
  --compress=xz \
  --fonts= \
  --locales= \
  --themes= \
  --install-modules="normal iso9660 echo multiboot2" \
  --modules="normal iso9660 echo multiboot2"

echo "ISO created: $ISO_NAME"
