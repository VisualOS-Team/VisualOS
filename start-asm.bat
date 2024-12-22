del boot.bin
del boot.img
del kernel.bin
del kernel.o

wsl nasm -f bin boot.asm -o boot.bin

wsl nasm -f bin kernel.asm -o kernel.bin

dd if=/dev/zero of=boot.img bs=512 count=2880
dd if=boot.bin of=boot.img bs=512 count=1 conv=notrunc
dd if=kernel.bin of=boot.img bs=512 seek=1 conv=notrunc

del boot.bin
del kernel.bin
del kernel.o

wsl qemu-system-i386 -drive format=raw,file=boot.img -d int,cpu_reset