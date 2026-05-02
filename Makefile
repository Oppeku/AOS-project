# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Oppeko

# --- Compiler & Toolchain ---
CC = gcc
AS = nasm
LD = ld
BUSYBOX_SRC ?= busybox
BUSYBOX_CC ?= musl-gcc
BUSYBOX_BINARY ?= /usr/bin/busybox
BUILD_BUSYBOX = scripts/build_busybox.sh
COREUTILS_BINARY ?=
GNU_C_DIR ?= GNU_C
GNU_COREUTILS_SRC ?= $(GNU_C_DIR)/coreutils-9.5/src
GNU_PROGRAMS ?= ls cat echo pwd uname head tail true false whoami mkdir
GNU_NANO_BINARY ?=
GNU_NANO_SRC ?= $(GNU_C_DIR)/nano/src/nano
GNU_NANO_ALT ?= $(GNU_C_DIR)/nano/nano
PARTITION_ALIASES ?= partition PartiotionMANAGAER PartiotionMANAGER PartitionMANAGER PartitionsMANAGER
GRUB_I386_DIR ?= /usr/lib/grub/i386-pc

# --- Flags ---
# -mno-sse/sse2: Prevents 'movaps' alignment crashes
# -mno-red-zone: Required for x86_64 kernels to prevent stack corruption
CFLAGS = -Iinclude -ffreestanding -O2 -Wall -Wextra \
         -fno-stack-protector -fno-pie -m64 \
         -mno-sse -mno-sse2 -mno-red-zone

ASFLAGS = -f elf64
LDFLAGS = -n -T scripts/linker.ld
USER_CFLAGS = -Iinclude -ffreestanding -O2 -Wall -Wextra \
              -fno-stack-protector -fpie -m64 -mno-red-zone \
              -fno-builtin

# --- Directories ---
SRC_DIR = .
BUILD_DIR = build
ISO_DIR = $(BUILD_DIR)/isofiles

# --- Object Files ---
# We use a wildcard to find all C and ASM files automatically
C_SOURCES = $(shell find kernel drivers -name "*.c")
ASM_SOURCES = $(shell find arch/x86_64 -name "*.asm")

OBJ = $(BUILD_DIR)/boot.o \
      $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(C_SOURCES))) \
      $(patsubst %.asm, $(BUILD_DIR)/%.o, $(notdir $(ASM_SOURCES)))

# Note: Since the above 'notdir' trick can cause name collisions, 
# you might need to list them manually if you have two files named 'utils.c' 
# in different folders. For now, let's keep it explicit based on your build:

OBJECTS = $(BUILD_DIR)/boot.o \
          $(BUILD_DIR)/main.o \
          $(BUILD_DIR)/vga.o \
          $(BUILD_DIR)/gdt.o \
          $(BUILD_DIR)/idt.o \
          $(BUILD_DIR)/pmm.o \
          $(BUILD_DIR)/vmm.o \
          $(BUILD_DIR)/keyboard.o \
          $(BUILD_DIR)/gdt_flush.o \
          $(BUILD_DIR)/interrupts.o \
          $(BUILD_DIR)/process.o \
          $(BUILD_DIR)/paging.o \
          $(BUILD_DIR)/syscall_c.o \
          $(BUILD_DIR)/vfs.o \
          $(BUILD_DIR)/tmpfs.o \
          $(BUILD_DIR)/fat32.o \
          $(BUILD_DIR)/ext4.o \
          $(BUILD_DIR)/partition.o \
          $(BUILD_DIR)/panic.o \
          $(BUILD_DIR)/syscall_entry.o \
          $(BUILD_DIR)/initrd.o \
          $(BUILD_DIR)/elf64_loader.o \
          $(BUILD_DIR)/process_c.o \
          $(BUILD_DIR)/timer.o

# --- Build Targets ---

all: $(BUILD_DIR)/aos.iso

$(BUILD_DIR)/aos.iso: $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/initrd.img $(BUILD_DIR)/fat32.img $(BUILD_DIR)/ext4.img
	@mkdir -p $(ISO_DIR)/boot/grub/i386-pc
	cp $(BUILD_DIR)/kernel.bin $(ISO_DIR)/boot/kernel.bin
	cp $(BUILD_DIR)/initrd.img $(ISO_DIR)/boot/initrd.img
	cp $(BUILD_DIR)/fat32.img $(ISO_DIR)/boot/fat32.img
	cp $(BUILD_DIR)/ext4.img $(ISO_DIR)/boot/ext4.img
	cp scripts/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	cp -r $(GRUB_I386_DIR)/. $(ISO_DIR)/boot/grub/i386-pc/
	grub-mkimage -O i386-pc -o $(ISO_DIR)/boot/grub/i386-pc/core.img -p /boot/grub iso9660 biosdisk multiboot2 normal configfile
	xorriso -as mkisofs -R -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -o $(BUILD_DIR)/aos.iso $(ISO_DIR)

$(BUILD_DIR)/initrd.img: hello.txt $(BUILD_DIR)/user.elf $(BUILD_DIR)/user2.elf $(BUILD_DIR)/shell.elf $(BUILD_DIR)/filetest.elf $(BUILD_DIR)/accesstest.elf $(BUILD_DIR)/openflagstest.elf $(BUILD_DIR)/duptest.elf $(BUILD_DIR)/pipetest.elf $(BUILD_DIR)/wait4test.elf $(BUILD_DIR)/stdincat.elf $(BUILD_DIR)/argvtest.elf $(BUILD_DIR)/pathtest.elf $(BUILD_DIR)/partitions.elf $(BUILD_DIR)/nano.elf $(BUILD_DIR)/gnu-nano $(BUILD_DIR)/busybox $(BUILD_DIR)/coreutils $(BUILD_DIR)/gnu-coreutils.stamp
	rm -rf $(BUILD_DIR)/initrd_root
	@mkdir -p $(BUILD_DIR)/initrd_root
	cp hello.txt $(BUILD_DIR)/initrd_root/hello.txt
	cp $(BUILD_DIR)/user.elf $(BUILD_DIR)/initrd_root/user.elf
	cp $(BUILD_DIR)/user2.elf $(BUILD_DIR)/initrd_root/user2.elf
	cp $(BUILD_DIR)/shell.elf $(BUILD_DIR)/initrd_root/shell.elf
	cp $(BUILD_DIR)/filetest.elf $(BUILD_DIR)/initrd_root/filetest.elf
	cp $(BUILD_DIR)/accesstest.elf $(BUILD_DIR)/initrd_root/accesstest.elf
	cp $(BUILD_DIR)/openflagstest.elf $(BUILD_DIR)/initrd_root/openflagstest.elf
	cp $(BUILD_DIR)/duptest.elf $(BUILD_DIR)/initrd_root/duptest.elf
	cp $(BUILD_DIR)/pipetest.elf $(BUILD_DIR)/initrd_root/pipetest.elf
	cp $(BUILD_DIR)/wait4test.elf $(BUILD_DIR)/initrd_root/wait4test.elf
	cp $(BUILD_DIR)/stdincat.elf $(BUILD_DIR)/initrd_root/stdincat.elf
	cp $(BUILD_DIR)/argvtest.elf $(BUILD_DIR)/initrd_root/argvtest.elf
	cp $(BUILD_DIR)/pathtest.elf $(BUILD_DIR)/initrd_root/pathtest.elf
	cp $(BUILD_DIR)/partitions.elf $(BUILD_DIR)/initrd_root/partitions.elf
	cp $(BUILD_DIR)/partitions.elf $(BUILD_DIR)/initrd_root/partitions
	for alias in $(PARTITION_ALIASES); do cp $(BUILD_DIR)/partitions.elf "$(BUILD_DIR)/initrd_root/$$alias"; done
	cp $(BUILD_DIR)/nano.elf $(BUILD_DIR)/initrd_root/aosnano.elf
	cp $(BUILD_DIR)/nano.elf $(BUILD_DIR)/initrd_root/aosnano
	if [ -f $(BUILD_DIR)/gnu-nano ]; then cp $(BUILD_DIR)/gnu-nano $(BUILD_DIR)/initrd_root/nano; else echo "GNU nano not found; /nano not installed. Put it at $(GNU_NANO_SRC) or set GNU_NANO_BINARY=..."; fi
	cp $(BUILD_DIR)/busybox $(BUILD_DIR)/initrd_root/busybox
	if [ -f $(BUILD_DIR)/coreutils ]; then cp $(BUILD_DIR)/coreutils $(BUILD_DIR)/initrd_root/coreutils; fi
	for prog in $(GNU_PROGRAMS); do if [ -f "$(BUILD_DIR)/gnu-coreutils/$$prog" ]; then cp "$(BUILD_DIR)/gnu-coreutils/$$prog" "$(BUILD_DIR)/initrd_root/$$prog"; fi; done
	cd $(BUILD_DIR)/initrd_root && { printf "hello.txt\nuser.elf\nuser2.elf\nshell.elf\nfiletest.elf\naccesstest.elf\nopenflagstest.elf\nduptest.elf\npipetest.elf\nwait4test.elf\nstdincat.elf\nargvtest.elf\npathtest.elf\npartitions.elf\npartitions\n"; for alias in $(PARTITION_ALIASES); do printf "%s\n" "$$alias"; done; printf "aosnano.elf\naosnano\nbusybox\n"; if [ -f nano ]; then printf "nano\n"; fi; if [ -f coreutils ]; then printf "coreutils\n"; fi; for prog in $(GNU_PROGRAMS); do if [ -f "$$prog" ]; then printf "%s\n" "$$prog"; fi; done; } | cpio -o -H newc > ../initrd.img

$(BUILD_DIR)/busybox: scripts/build_busybox.sh scripts/prepare_busybox.py
	@mkdir -p $(BUILD_DIR)
	@if [ -d "$(BUSYBOX_SRC)" ]; then \
		BUSYBOX_CC="$(BUSYBOX_CC)" $(BUILD_BUSYBOX) "$(BUSYBOX_SRC)" $@; \
	elif [ -f "$(BUSYBOX_BINARY)" ]; then \
		python3 scripts/prepare_busybox.py "$(BUSYBOX_BINARY)" $@; \
	else \
		echo "busybox source tree not found: $(BUSYBOX_SRC)" >&2; \
		echo "busybox binary not found: $(BUSYBOX_BINARY)" >&2; \
		exit 1; \
	fi

$(BUILD_DIR)/coreutils:
	@mkdir -p $(BUILD_DIR)
	@if [ -n "$(COREUTILS_BINARY)" ]; then \
		python3 scripts/prepare_busybox.py "$(COREUTILS_BINARY)" $@; \
	elif [ -f "$(GNU_C_DIR)/coreutils" ]; then \
		python3 scripts/prepare_busybox.py "$(GNU_C_DIR)/coreutils" $@; \
	elif [ -f "$(GNU_C_DIR)/busybox" ]; then \
		python3 scripts/prepare_busybox.py "$(GNU_C_DIR)/busybox" $@; \
	elif [ -f "$(GNU_C_DIR)/src/coreutils" ]; then \
		python3 scripts/prepare_busybox.py "$(GNU_C_DIR)/src/coreutils" $@; \
	else \
		rm -f $@; \
	fi

$(BUILD_DIR)/gnu-coreutils.stamp: scripts/prepare_busybox.py
	@mkdir -p $(BUILD_DIR)/gnu-coreutils
	@found=0; \
	for prog in $(GNU_PROGRAMS); do \
		if [ -f "$(GNU_COREUTILS_SRC)/$$prog" ]; then \
			python3 scripts/prepare_busybox.py "$(GNU_COREUTILS_SRC)/$$prog" "$(BUILD_DIR)/gnu-coreutils/$$prog" >/dev/null; \
			found=1; \
		else \
			rm -f "$(BUILD_DIR)/gnu-coreutils/$$prog"; \
		fi; \
	done; \
	if [ "$$found" -eq 1 ]; then touch $@; else rm -f $@; fi

$(BUILD_DIR)/gnu-nano: scripts/prepare_busybox.py
	@mkdir -p $(BUILD_DIR)
	@if [ -n "$(GNU_NANO_BINARY)" ]; then \
		python3 scripts/prepare_busybox.py "$(GNU_NANO_BINARY)" $@; \
	elif [ -f "$(GNU_NANO_SRC)" ]; then \
		python3 scripts/prepare_busybox.py "$(GNU_NANO_SRC)" $@; \
	elif [ -f "$(GNU_NANO_ALT)" ]; then \
		python3 scripts/prepare_busybox.py "$(GNU_NANO_ALT)" $@; \
	else \
		rm -f $@; \
	fi

$(BUILD_DIR)/fat32.img: hello.txt
	@mkdir -p $(BUILD_DIR)/fat32_root
	cp hello.txt $(BUILD_DIR)/fat32_root/HELLO.TXT
	printf "AOS FAT32 test image\n" > $(BUILD_DIR)/fat32_root/README.TXT
	dd if=/dev/zero of=$@ bs=1M count=16 status=none
	mkfs.fat -F 32 -n AOSFAT32 $@
	mcopy -i $@ $(BUILD_DIR)/fat32_root/HELLO.TXT ::HELLO.TXT
	mcopy -i $@ $(BUILD_DIR)/fat32_root/README.TXT ::README.TXT

$(BUILD_DIR)/ext4.img: hello.txt
	@mkdir -p $(BUILD_DIR)/ext4_root
	cp hello.txt $(BUILD_DIR)/ext4_root/hello.txt
	printf "AOS EXT4 test image\n" > $(BUILD_DIR)/ext4_root/readme.txt
	dd if=/dev/zero of=$@ bs=1M count=16 status=none
	mkfs.ext4 -q -F -L AOSEXT4 -O ^has_journal,^metadata_csum,^64bit,^dir_index -d $(BUILD_DIR)/ext4_root $@

$(BUILD_DIR)/user.o: userspace/user.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/user.elf: $(BUILD_DIR)/user.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/user.o

$(BUILD_DIR)/shell.o: userspace/shell.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/shell.elf: $(BUILD_DIR)/shell.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/shell.o

$(BUILD_DIR)/user2.o: userspace/user2.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/user2.elf: $(BUILD_DIR)/user2.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/user2.o

$(BUILD_DIR)/filetest.o: userspace/filetest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/filetest.elf: $(BUILD_DIR)/filetest.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/filetest.o

$(BUILD_DIR)/accesstest.o: userspace/accesstest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/accesstest.elf: $(BUILD_DIR)/accesstest.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/accesstest.o

$(BUILD_DIR)/openflagstest.o: userspace/openflagstest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/openflagstest.elf: $(BUILD_DIR)/openflagstest.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/openflagstest.o

$(BUILD_DIR)/duptest.o: userspace/duptest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/duptest.elf: $(BUILD_DIR)/duptest.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/duptest.o

$(BUILD_DIR)/pipetest.o: userspace/pipetest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/pipetest.elf: $(BUILD_DIR)/pipetest.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/pipetest.o

$(BUILD_DIR)/wait4test.o: userspace/wait4test.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/wait4test.elf: $(BUILD_DIR)/wait4test.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/wait4test.o

$(BUILD_DIR)/stdincat.o: userspace/stdincat.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/stdincat.elf: $(BUILD_DIR)/stdincat.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/stdincat.o

$(BUILD_DIR)/argvtest.o: userspace/argvtest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/argvtest.elf: $(BUILD_DIR)/argvtest.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/argvtest.o

$(BUILD_DIR)/pathtest.o: userspace/pathtest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/pathtest.elf: $(BUILD_DIR)/pathtest.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/pathtest.o

$(BUILD_DIR)/partitions.o: userspace/partitions.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/partitions.elf: $(BUILD_DIR)/partitions.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/partitions.o

$(BUILD_DIR)/nano.o: userspace/nano.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/nano.elf: $(BUILD_DIR)/nano.o userspace/user.ld
	$(LD) -nostdlib -pie -T userspace/user.ld -o $@ $(BUILD_DIR)/nano.o

$(BUILD_DIR)/kernel.bin: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# --- Rules for Object Files ---

$(BUILD_DIR)/boot.o: arch/x86_64/boot/boot.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# Assembly rules
$(BUILD_DIR)/syscall_entry.o: arch/x86_64/kernel/syscall.asm
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/interrupts.o: arch/x86_64/kernel/interrupts.asm
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/process.o: arch/x86_64/kernel/process.asm
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/gdt_flush.o: arch/x86_64/kernel/gdt_flush.asm
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/paging.o: arch/x86_64/mm/paging.asm
	$(AS) $(ASFLAGS) $< -o $@

# C rules
$(BUILD_DIR)/main.o: kernel/main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vga.o: drivers/video/vga.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/gdt.o: kernel/gdt.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: kernel/idt.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pmm.o: kernel/pmm.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vmm.o: kernel/vmm.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: drivers/char/keyboard.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_c.o: kernel/syscall.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vfs.o: kernel/vfs.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tmpfs.o: kernel/tmpfs.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fat32.o: kernel/fat32.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ext4.o: kernel/ext4.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/partition.o: kernel/partition.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/panic.o: kernel/panic.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/initrd.o: kernel/initrd.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/elf64_loader.o: kernel/elf64_loader.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/process_c.o: kernel/process.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/timer.o: kernel/timer.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Utility ---

clean:
	rm -rf $(BUILD_DIR)

run: all
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/aos.iso -display gtk -serial stdio -d int -D qemu.log

run-headless: all
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/aos.iso -serial stdio -display none -d int -D qemu.log

run-curses: all
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/aos.iso -display curses -serial none -d int -D qemu.log

.PHONY: all clean run run-headless run-curses
