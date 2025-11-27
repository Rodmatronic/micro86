make clean; make -j15
echo -e "\n#\n#\n#  Kernel done. Building the ISO image.\n#\n#\n"
bash build/makeiso.sh
make qemu
