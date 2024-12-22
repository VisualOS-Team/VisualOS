dd if=/dev/zero of=boot.img bs=512 count=2880
dd if=boot.bin of=boot.img bs=512 count=1 conv=notrunc
dd if=kernel.bin of=boot.img bs=512 seek=1 conv=notrunc