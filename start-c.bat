del boot.bin
del boot.img
del loader.bin
del kernel.bin
del kernel.o

wsl nasm -f bin boot.asm -o boot.bin
wsl nasm -f bin loader.asm -o loader.bin

wsl gcc -m32 -ffreestanding -c kernel.c -o kernel.o
wsl ld -T linker.ld -o kernel.bin --oformat binary kernel.o

dd if=/dev/zero of=boot.img bs=512 count=2880
dd if=boot.bin of=boot.img bs=512 count=1 conv=notrunc
dd if=loader.bin of=boot.img bs=512 count=1 conv=notrunc
dd if=kernel.bin of=boot.img bs=512 seek=1 conv=notrunc

del boot.bin
del kernel.bin
del kernel.o
del loader.bin

wsl qemu-system-i386 -drive format=raw,file=boot.img -d int,cpu_reset,guest_errors,pcall,trace:all
