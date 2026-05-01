============================================================
       AOS v0.1 - 64-Bit Operating System Kernel
       Developer: Abhigyan
       License: GNU GPLv3.0
============================================================

PROJECT OVERVIEW:
AOS is a custom-built, 64-bit x86_64 operating system kernel. 
It features a full transition into Long Mode, identity-mapped 
paging, a custom Interrupt Descriptor Table (IDT), and a 
functional Keyboard Driver with a US-QWERTY mapping.

------------------------------------------------------------
1. DIRECTORY STRUCTURE
------------------------------------------------------------
/build              <-- Contains 'aos.iso' (The bootable OS)
/kernel             <-- Core C logic (main, IDT setup)
/drivers            <-- Hardware drivers (VGA, Keyboard)
/arch               <-- Bootloader and Paging assembly
/license            <-- Official GPLv3.0 License text
/scripts            <-- Linker and GRUB configuration
README.txt          <-- This guide

------------------------------------------------------------
2. HOW TO RUN (WINDOWS)
------------------------------------------------------------
The kernel is pre-compiled into a bootable ISO image. 

STEP 1: Locate the 'aos.iso' file inside the [build] folder.

STEP 2: Use QEMU for Windows (Recommended).
        Open PowerShell or CMD in this directory and run:
        
        qemu-system-x86_64 -cdrom ./build/aos.iso

STEP 3: Use VirtualBox (Alternative).
        - Create a New VM (Type: Other, Version: Other 64-bit).
        - Assign 512MB RAM.
        - Under Settings -> Storage, mount 'aos.iso' to the CD drive.
        - Start the VM.

------------------------------------------------------------
3. TECHNICAL SPECIFICATIONS
------------------------------------------------------------
* Architecture: x86_64 (64-bit Long Mode).
* Memory: 2MB Huge Pages (Identity Mapped).
* Interrupts: PIC Remapped (IRQ 32-47) to prevent collisions 
  with CPU exceptions.
* I/O: Port-based communication (Port 0x60 for Keyboard).
* License: FOSS (Free and Open Source Software).

------------------------------------------------------------
4. USAGE INSTRUCTIONS
------------------------------------------------------------
Once the kernel boots and displays "64-bit Mode Active":
1. Type any key on your keyboard.
2. The custom driver will capture the scancode and map it 
   to the correct ASCII character on the VGA display.

============================================================
