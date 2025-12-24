# Variables for compiler and flags
CC = gcc
AS = nasm
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -std=gnu99
LDFLAGS = -T linker.ld -m32 -nostdlib -ffreestanding -Wl,--build-id=none -no-pie

# Object files to be linked
# Add keyboard.o if you moved the keyboard logic to a separate file
OBJ = boot.o kernel.o font.o vesa.o io.o lib.o idt.o interrupts.o gdt.o gdt_flush.o shell.o pmm.o paging.o paging_asm.o task.o kheap.o fs.o

all: myos.iso

interrupts.o: interrupts.s
	$(AS) -f elf32 interrupts.s -o interrupts.o

paging_asm.o: paging_asm.s
	$(AS) -f elf32 $< -o $@



# Linking the final binary
myos.bin: $(OBJ)
	$(CC) $(LDFLAGS) -o myos.bin $(OBJ)

# Compiling boot assembly
boot.o: boot.s
	$(AS) -f elf32 boot.s -o boot.o

# Compiling C files
# The -c flag tells gcc to compile but not link yet
kernel.o: kernel.c vesa.h io.h
	$(CC) -c kernel.c -o kernel.o $(CFLAGS)

fs.o: fs.c fs.h
	$(CC) -c fs.c -o fs.o $(CFLAGS)



kheap.o: kheap.c kheap.h 
	$(CC) -c kheap.c -o kheap.o $(CFLAGS)

font.o: font.c font.h
	$(CC) -c font.c -o font.o $(CFLAGS)


task.o: task.c task.h io.h
	$(CC) -c task.c -o task.o $(CFLAGS)


io.o: io.c io.h
	$(CC) -c io.c -o io.o $(CFLAGS)

vesa.o: vesa.c vesa.h io.h
	$(CC) -c vesa.c -o vesa.o $(CFLAGS)

%.o: %.c
	$(CC) -c $^ -o $@ $(CFLAGS)
# New rule for the GDT C code
gdt.o: gdt.c gdt.h
	$(CC) -c gdt.c -o gdt.o $(CFLAGS)

pmm.o: pmm.c lib.h pmm.h
	$(CC) -c pmm.c -o pmm.o $(CFLAGS)



shell.o: shell.c shell.h
	$(CC) -c shell.c -o shell.o $(CFLAGS)
# New rule for the GDT assembly flush
gdt_flush.o: gdt_flush.s
	$(AS) -f elf32 gdt_flush.s -o gdt_flush.o
# If you create keyboard.c, add this rule:
# keyboard.o: keyboard.c io.h vesa.h
# 	$(CC) -c keyboard.c -o keyboard.o $(CFLAGS)

# Building the ISO
myos.iso: myos.bin
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/myos.bin
	echo 'set timeout=0' > isodir/boot/grub/grub.cfg
	echo 'set default=0' >> isodir/boot/grub/grub.cfg
	echo 'menuentry "myos" { multiboot /boot/myos.bin }' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso isodir

check:
	grub-file --is-x86-multiboot myos.bin

run:
	qemu-system-i386 -cdrom myos.iso -boot d

clean:
	rm -rf *.o myos.bin myos.iso isodir
