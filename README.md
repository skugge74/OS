# KDXOS

KDXOS is a 32-bit graphical, multitasking operating system built from scratch in C and Assembly. Developed as a hobbyist architecture project, it features a dynamic kernel heap, a FAT-compatible file system, a tiling window manager, and a custom Turing-complete programming language. Instead of relying on standard C executables, KDXOS includes an on-device compiler that translates native scripts directly into x86 machine code binaries within the operating system.

## Core Features

### Kernel & Architecture

* **Boot Process:** Multiboot compliant, booting via GRUB into a 32-bit protected mode environment.
* **Memory Management:** Full virtual memory mapping and isolation via a Physical Memory Manager (PMM) and custom paging structure.
* **Dynamic Kernel Heap:** Custom `kmalloc` and `kfree` implementation managing a 64MB pool, complete with automatic block coalescing and internal fragmentation mitigation.
* **Preemptive Multitasking:** Hardware timer-driven context switching, round-robin task scheduling, and isolated 16KB stacks per process.

### Graphics & Window Management

* **VESA Driver:** Double-buffered rendering supporting high-resolution output (e.g., 1024x768).
* **Tiling Compositor:** A dedicated background task handling dynamic screen tiling, window redrawing, and active-focus tracking for spawned GUI tasks.
* **Render Optimization:** Implements smart dirty rectangles to redraw only updated screen regions, minimizing computational overhead.

### Storage & File System

* **Hardware Drivers:** Custom PIO-mode ATA disk driver for IDE hard drives.
* **FAT-Hybrid Implementation:** Native support for file and directory operations (create, read, write, append, delete).
* **Path Management:** Full support for subdirectories, relative pathing (`.` and `..`), and absolute path parsing.
* **Dynamic Allocation:** Automatically expands files across the disk by allocating free clusters and updating the File Allocation Table.

### Userland & Shell

The operating system boots into an interactive, graphical command-line interface with a suite of built-in commands:

* **File I/O:** `ls`, `cd`, `cat`, `mkdir`, `rm`, `rmdir`, `pwd`, `touch`, `write`, `hexdump`
* **Process Control:** `ps`, `kill`, `top`, `run`
* **System Diagnostics:** `stat` (live kernel heap analysis), `uptime`, `clear`, `echo`
* **Included Applications:** `ked` (Native Text Editor), `game`, `timer`

### Native Toolchain & Scripting

KDXOS features a built-in assembler/compiler that allows users to write programs in the custom KDXOS Scripting Language and compile them directly on the device.

* **Control Flow:** Turing-complete support for `GOTO`, `CALL`, `RET`, and conditional jumps (`CMP`, `JE`, `JL`, `JG`, `JNE`).
* **Logic & Variables:** Dynamic runtime variable allocation (`SET`) and arithmetic (`ADD`, `SUB`).
* **I/O & Graphics:** Non-blocking hardware keyboard hooks (`GETKEY`) and direct compositor API calls (`WINDOW`, `RECT`, `PRINT`).

## Code Example

A standard KDXOS application rendering an interactive menu and listening for hardware interrupts:

```as
WINDOW 0 0 400 300
CALL draw_menu
SET key 0

LABEL input_loop
    GETKEY key
    CMP key 0
    JE loop_end
    
    CMP key 113  # 'q' to quit
    JE handle_quit
    
LABEL loop_end
    SLEEP 50
    GOTO input_loop

LABEL handle_quit
    RECT 20 150 360 40 0x222222
    PRINT "Exiting application..." 30 160 0xFF0000
    SLEEP 1000
    EXIT

LABEL draw_menu
    RECT 0 0 400 300 0x222222
    PRINT "KDXOS INTERACTIVE MENU" 100 20 0xFFFF00
    PRINT "[Q] Quit Application" 40 130 0xAAAAAA
    RET

```

## Building & Running

### Prerequisites

* `gcc` (i686-elf cross-compiler recommended)
* `nasm`
* `qemu-system-i386` (for emulation)
* `make`

### Compilation

To compile the kernel and build the OS image:

```bash
make clean
make

```

### Emulation

To launch KDXOS inside QEMU with an attached IDE hard drive image:

```bash
make run

```

## Project Structure

```text
OS_Root/
├── assets/             # External media and bitmap fonts
├── bin/                # Compiled userland binaries (.BIN)
├── build/              # Generated object files (.o)
├── disk.img            # The compiled FAT filesystem hard drive image
├── file/               # Raw files injected into the disk image on build
├── include/            # C headers (.h) for kernel and drivers
├── tests/              # Testing framework and scripts
├── linker.ld           # Linker script for kernel memory mapping
├── Makefile            # Build system rules
├── sys_specs.csv       # Language and Syscall documentation
└── src/                # Kernel and Driver Source Code
    ├── assembler.c     # Native KDXOS compiler
    ├── bmp.c           # BMP parsing and rendering
    ├── boot.s          # Multiboot entry point
    ├── fat.c           # FAT File System and ATA PIO driver
    ├── font.c          # Bitmap font rendering
    ├── gdt.c           # Global Descriptor Table setup
    ├── gdt_flush.s     # GDT assembly loader
    ├── idt.c           # Interrupt Descriptor Table & Exceptions
    ├── interrupts.s    # ISR and IRQ assembly wrappers
    ├── io.c            # Low-level port I/O
    ├── KED.c           # Native text editor application
    ├── kernel.c        # Main kernel entry point
    ├── kheap.c         # Dynamic Memory Manager
    ├── lib.c           # Standard library utilities
    ├── paging.c        # Virtual memory management
    ├── paging_asm.s    # Paging assembly routines
    ├── pmm.c           # Physical Memory Manager
    ├── shell.c         # Graphical CLI environment
    ├── task.c          # Preemptive multitasking scheduler
    └── vesa.c          # VESA Graphics driver

```

## Roadmap

* [ ] Virtual File System (VFS) abstraction layer.
* [ ] Extended standard library for the native scripting language.

---

*Built with C, Assembly, and a lot of kernel panics.*
