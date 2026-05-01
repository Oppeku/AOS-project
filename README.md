----------------------------------------------------------------------------------------------
#AOS
---------------------------------------------------------------------------------------------

AOS is a hobby x86_64 operating system kernel written mostly in C and Assembly.

It is built to learn how operating systems actually work by building one from scratch.

Also before anyone asks:

AOS does not stand for anything.
It is just called AOS.
-----------------------------------------------------------------------------------------------
1. Why This Exists
-----------------------------------------------------------------------------------------------

Modern operating systems are full of things running in the background that users never asked for.

AOS tries a different idea:

keep things simple
keep things fast
keep things understandable
avoid bloat

The goal is to build a system that is easy to understand and easy to use.

Or in simpler terms:

If the OS starts doing weird stuff behind your back, it is probably not AOS.

Current Features
Kernel
Physical Memory Manager
Virtual Memory Manager
GDT and IDT
Hardware interrupts
System timer
Panic handler
Process System
fork
execve
ELF64 loader
Userspace stack mapping

Yes, real programs can run.
-----------------------------------------------------------
2. Filesystems
----------------------------------------------------------
AOS currently supports:

FAT32 (read/write)
EXT4 (read/write)
tmpfs
initrd
Boot
Multiboot2 kernel
GRUB bootloader
initrd using CPIO
Userspace

Includes a small interactive shell and several syscall test programs.

Programs currently included:

shell
nano
filetest
accesstest
openflagstest
duptest
pipetest
wait4test
argvtest
pathtest

BusyBox is also included to provide common Unix utilities.

Example Boot
Starting physical memory manager [ OK ]
Starting virtual memory manager [ OK ]
Starting global descriptor table [ OK ]
Starting interrupt descriptor table [ OK ]
Starting process management [ OK ]
Starting VFS mount table [ OK ]

Starting FAT32 mount at /mnt/fat32 [ OK ]
Starting EXT4 mount at /mnt/ext4 [ OK ]

--- AOS Interactive Shell v0.1 ---
Type help for commands.

Example usage:

AOS# ls
AOS# cd /mnt/ext4
AOS# cat readme.txt
AOS# busybox
Repository Structure
arch/        architecture specific code
drivers/     device drivers
kernel/      core kernel code
include/     kernel headers
userspace/   userspace programs
scripts/     build scripts
docs/        documentation
GNU_C/       GNU utilities
Building
-----------------------------------------------
3. Requirements:
----------------------------------------------
gcc
nasm
grub
xorriso
mkfs.fat
mkfs.ext4
qemu
-----------------------------------------------
4. Build the OS:
-------------------------------------------------
make

Run it:

make run

This launches AOS in QEMU.
------------------------------------------------
5. Python
-------------------------------------------------
AOS itself is Python-free.

The kernel and userspace programs are written only in:

C
Assembly

However there is one Python script in the repository:

scripts/prepare_busybox.py

This script is used only during the build process.

And for honesty:

The Python script was written with help from the internet.

It does not run inside the operating system.
-----------------------------------------------------
6. Important Note
------------------------------------------------------

Things may break.
Things may crash.
Sometimes things will work for no clear reason.

That is normal when building an operating system.
-----------------------------------------------------
7. License
-----------------------------------------------------
This project is licensed under the GNU General Public License v3 (GPLv3).

See the LICENSE file for details.
---------
8. Author
--------
Email id - abhigyannarayan653@gmail.com
Created by Abhigyan.
------------------------------------------

#MAN JUST HOW HARD IS IT TO BE FORMAL 
#BTW DID U WATCH THE LATEST ONE PIECE ELBAF ARC ITS FIRE WITH HIGH LVL ANIMATIONS 
