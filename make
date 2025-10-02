make clean; make -j15; make image
bash build/makeiso.sh
make qemu-memfs
