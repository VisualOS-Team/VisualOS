@echo off
del boot.img
wsl nasm -f bin boot.asm -o boot.bin -l boot.lst
wsl nasm -f bin loader.asm -o loader.bin -l loader.lst
wsl gcc -m32 -ffreestanding -c kernel.c -o kernel.o -v
wsl ld -T linker.ld -o kernel.bin --oformat binary kernel.o -Map=kernel.map
wsl dd if=/dev/zero of=boot.img bs=512 count=2880
wsl dd if=boot.bin of=boot.img bs=512 count=1 conv=notrunc
wsl dd if=loader.bin of=boot.img bs=512 count=1 conv=notrunc
wsl dd if=kernel.bin of=boot.img bs=512 seek=1 conv=notrunc
wsl qemu-system-i386 -drive format=raw,file=boot.img -d cpu_reset,int,exec,guest_errors -D debug.log -monitor stdio