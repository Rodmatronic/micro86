#!/bin/sh
# makeiso.sh - create a simple GRUB bootable ISO with a file 'frunix'

set -e

ISO_NAME="frunix.iso"
WORKDIR="isotree"

# Clean previous build
rm -rf "$WORKDIR" "$ISO_NAME"
mkdir -p "$WORKDIR/boot/grub"

# Create a minimal GRUB config
cat > "$WORKDIR/boot/grub/grub.cfg" <<EOF
set timeout=5
set default=0

menuentry "Boot from frunix (chainload test)" {
	multiboot /frunix
}
EOF

# Copy the custom file to the root of the ISO
cp sys/frunix "$WORKDIR/"

# Build the ISO
grub-mkrescue -o "$ISO_NAME" "$WORKDIR"

echo "ISO created: $ISO_NAME"

