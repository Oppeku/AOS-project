# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Oppeko

# --- Compiler & Toolchain ---
CC = gcc
AS = nasm
LD = ld
HOSTCC ?= cc
LINUX_LD_SO ?= /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
LINUX_LIBC_SO ?= /lib/x86_64-linux-gnu/libc.so.6
PARTITION_ALIASES ?= partition PartiotionMANAGAER PartiotionMANAGER PartitionMANAGER PartitionsMANAGER
ASHEEL_ALIASES ?= sh ash test [ printf env grep find sed awk chmod chown ln mount umount df du tar gzip kill pause resume sleep dmesg cat echo true false pwd ls date stat head tail touch rm mkdir rmdir cp mv uname whoami id uptime mem
ASENCE_ALIASES ?= Asence asence Asnece asnece
GRUB_I386_DIR ?= /usr/lib/grub/i386-pc
GRUB_X86_64_EFI_DIR ?= /usr/lib/grub/x86_64-efi
AOSFS_IMAGE_SIZE_MB ?= 16
AOS_LIVE_DISK_SIZE_MB ?= 64
AOS_BOOT_VERBOSE ?= 1
AOS_LIVE_PERMISSIVE ?= 1
AOS_ENABLE_MUI_FRAMEBUFFER ?= 1
AOSFS_DRIVE_ARGS = -drive file=$(BUILD_DIR)/live-disk.img,format=raw,if=ide,index=0,media=disk
AOS_NET_ARGS ?= -netdev user,id=aosnet -device e1000,netdev=aosnet
AOS_USB_ARGS ?= -device qemu-xhci,id=aosxhci -device usb-tablet,bus=aosxhci.0

# --- Flags ---
# -mno-sse/sse2: Prevents 'movaps' alignment crashes
# -mno-red-zone: Required for x86_64 kernels to prevent stack corruption
CFLAGS = -Iinclude -ffreestanding -O2 -Wall -Wextra \
         -fno-stack-protector -fno-pie -m64 \
         -mno-sse -mno-sse2 -mno-red-zone \
         -DAOS_BOOT_VERBOSE=$(AOS_BOOT_VERBOSE) \
         -DAOS_LIVE_PERMISSIVE=$(AOS_LIVE_PERMISSIVE) \
         -DAOS_ENABLE_MUI_FRAMEBUFFER=$(AOS_ENABLE_MUI_FRAMEBUFFER)

ASFLAGS = -f elf64
LDFLAGS = -n -T scripts/linker.ld
USER_CFLAGS = -Iinclude -ffreestanding -O2 -Wall -Wextra \
              -fno-stack-protector -fpie -m64 \
              -mno-sse -mno-sse2 -mno-red-zone \
              -fno-builtin
USER_LDFLAGS = -nostdlib -pie --no-dynamic-linker

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
          $(BUILD_DIR)/gfx.o \
          $(BUILD_DIR)/gdt.o \
          $(BUILD_DIR)/idt.o \
          $(BUILD_DIR)/pmm.o \
          $(BUILD_DIR)/vmm.o \
          $(BUILD_DIR)/keyboard.o \
          $(BUILD_DIR)/mouse.o \
          $(BUILD_DIR)/gdt_flush.o \
          $(BUILD_DIR)/interrupts.o \
          $(BUILD_DIR)/process.o \
          $(BUILD_DIR)/paging.o \
          $(BUILD_DIR)/syscall_c.o \
          $(BUILD_DIR)/syscall_ipc.o \
          $(BUILD_DIR)/syscall_process_lifecycle.o \
          $(BUILD_DIR)/syscall_memory.o \
          $(BUILD_DIR)/syscall_terminal_io.o \
          $(BUILD_DIR)/syscall_fs.o \
          $(BUILD_DIR)/syscall_process_info.o \
          $(BUILD_DIR)/syscall_network.o \
          $(BUILD_DIR)/syscall_linux_misc.o \
          $(BUILD_DIR)/syscall_aos_services.o \
          $(BUILD_DIR)/syscall_dispatch.o \
          $(BUILD_DIR)/vfs.o \
          $(BUILD_DIR)/blkdev.o \
          $(BUILD_DIR)/pci.o \
          $(BUILD_DIR)/driver.o \
          $(BUILD_DIR)/input.o \
          $(BUILD_DIR)/firmware.o \
          $(BUILD_DIR)/mac80211.o \
          $(BUILD_DIR)/netdev.o \
          $(BUILD_DIR)/ata.o \
          $(BUILD_DIR)/e1000.o \
          $(BUILD_DIR)/wifi_driver.o \
          $(BUILD_DIR)/bluetooth.o \
          $(BUILD_DIR)/xhci.o \
          $(BUILD_DIR)/aosfs.o \
          $(BUILD_DIR)/tmpfs.o \
          $(BUILD_DIR)/fat32.o \
          $(BUILD_DIR)/ext4.o \
          $(BUILD_DIR)/partition.o \
          $(BUILD_DIR)/panic.o \
          $(BUILD_DIR)/syscall_entry.o \
          $(BUILD_DIR)/initrd.o \
          $(BUILD_DIR)/elf64_loader.o \
          $(BUILD_DIR)/process_c.o \
          $(BUILD_DIR)/timer.o \
          $(BUILD_DIR)/thermal.o \
          $(BUILD_DIR)/credentials.o \
          $(BUILD_DIR)/session.o \
          $(BUILD_DIR)/installer.o \
          $(BUILD_DIR)/tty.o \
          $(BUILD_DIR)/rtc.o

# --- Build Targets ---

all: $(BUILD_DIR)/aos.iso

$(BUILD_DIR)/linux-dynamic-hello: tests/linux_dynamic_hello.c
	@mkdir -p $(BUILD_DIR)
	$(HOSTCC) -O2 -Wall -Wextra -fPIE -pie \
		-Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2 -o $@ $<

$(BUILD_DIR)/linux-dynamic-launcher.o: tests/linux_dynamic_launcher.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/linux-dynamic-launcher.elf: $(BUILD_DIR)/linux-dynamic-launcher.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $<

$(BUILD_DIR)/uni-install-launcher.o: tests/uni_install_launcher.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/uni-install-launcher.elf: $(BUILD_DIR)/uni-install-launcher.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $<

$(BUILD_DIR)/aos-linux-dynamic-test.iso: $(BUILD_DIR)/aos.iso $(BUILD_DIR)/linux-dynamic-launcher.elf
	rm -rf $(BUILD_DIR)/linux_dynamic_test_initrd $(BUILD_DIR)/linux_dynamic_test_isofiles
	cp -a $(BUILD_DIR)/initrd_root $(BUILD_DIR)/linux_dynamic_test_initrd
	cp $(BUILD_DIR)/linux-dynamic-launcher.elf $(BUILD_DIR)/linux_dynamic_test_initrd/desktop.elf
	cd $(BUILD_DIR)/linux_dynamic_test_initrd && find . -mindepth 1 -printf '%P\n' | LC_ALL=C sort | cpio -o -H newc > ../initrd-linux-dynamic-test.img
	cp -a $(ISO_DIR) $(BUILD_DIR)/linux_dynamic_test_isofiles
	cp $(BUILD_DIR)/initrd-linux-dynamic-test.img $(BUILD_DIR)/linux_dynamic_test_isofiles/boot/initrd.img
	xorriso -as mkisofs -R -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -o $@ $(BUILD_DIR)/linux_dynamic_test_isofiles

$(BUILD_DIR)/aos-uni-test.iso: $(BUILD_DIR)/aos.iso $(BUILD_DIR)/uni-install-launcher.elf
	rm -rf $(BUILD_DIR)/uni_test_initrd $(BUILD_DIR)/uni_test_isofiles
	cp -a $(BUILD_DIR)/initrd_root $(BUILD_DIR)/uni_test_initrd
	cp $(BUILD_DIR)/uni-install-launcher.elf $(BUILD_DIR)/uni_test_initrd/desktop.elf
	cd $(BUILD_DIR)/uni_test_initrd && find . -mindepth 1 -printf '%P\n' | LC_ALL=C sort | cpio -o -H newc > ../initrd-uni-test.img
	cp -a $(ISO_DIR) $(BUILD_DIR)/uni_test_isofiles
	cp $(BUILD_DIR)/initrd-uni-test.img $(BUILD_DIR)/uni_test_isofiles/boot/initrd.img
	xorriso -as mkisofs -R -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -o $@ $(BUILD_DIR)/uni_test_isofiles

$(BUILD_DIR)/aos-hello.deb: pakages/aos-hello.control README.md $(BUILD_DIR)/echo.elf
	rm -rf $(BUILD_DIR)/uni_fixture
	mkdir -p $(BUILD_DIR)/uni_fixture/control $(BUILD_DIR)/uni_fixture/data/usr/bin $(BUILD_DIR)/uni_fixture/data/usr/share/doc/aos-hello $(BUILD_DIR)/uni_fixture/ar
	cp pakages/aos-hello.control $(BUILD_DIR)/uni_fixture/control/control
	cp $(BUILD_DIR)/echo.elf $(BUILD_DIR)/uni_fixture/data/usr/bin/aos-hello
	cp README.md $(BUILD_DIR)/uni_fixture/data/usr/share/doc/aos-hello/README.md
	tar --format=ustar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -cf $(BUILD_DIR)/uni_fixture/ar/control.tar -C $(BUILD_DIR)/uni_fixture/control .
	tar --format=ustar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -cf $(BUILD_DIR)/uni_fixture/ar/data.tar -C $(BUILD_DIR)/uni_fixture/data .
	gzip -n -9 $(BUILD_DIR)/uni_fixture/ar/control.tar
	gzip -n -9 $(BUILD_DIR)/uni_fixture/ar/data.tar
	printf '2.0\n' > $(BUILD_DIR)/uni_fixture/ar/debian-binary
	cd $(BUILD_DIR)/uni_fixture/ar && ar rcD ../../aos-hello.deb debian-binary control.tar.gz data.tar.gz

$(BUILD_DIR)/aos-hello-xz.deb: pakages/aos-hello.control README.md $(BUILD_DIR)/echo.elf
	rm -rf $(BUILD_DIR)/uni_fixture_xz
	mkdir -p $(BUILD_DIR)/uni_fixture_xz/control $(BUILD_DIR)/uni_fixture_xz/data/usr/bin $(BUILD_DIR)/uni_fixture_xz/data/usr/share/doc/aos-hello $(BUILD_DIR)/uni_fixture_xz/ar
	cp pakages/aos-hello.control $(BUILD_DIR)/uni_fixture_xz/control/control
	cp $(BUILD_DIR)/echo.elf $(BUILD_DIR)/uni_fixture_xz/data/usr/bin/aos-hello
	cp README.md $(BUILD_DIR)/uni_fixture_xz/data/usr/share/doc/aos-hello/README.md
	tar --format=ustar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -cf $(BUILD_DIR)/uni_fixture_xz/ar/control.tar -C $(BUILD_DIR)/uni_fixture_xz/control .
	tar --format=ustar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -cf $(BUILD_DIR)/uni_fixture_xz/ar/data.tar -C $(BUILD_DIR)/uni_fixture_xz/data .
	xz -f -9 --check=crc64 --threads=1 $(BUILD_DIR)/uni_fixture_xz/ar/control.tar
	xz -f -9 --check=crc64 --threads=1 $(BUILD_DIR)/uni_fixture_xz/ar/data.tar
	printf '2.0\n' > $(BUILD_DIR)/uni_fixture_xz/ar/debian-binary
	cd $(BUILD_DIR)/uni_fixture_xz/ar && ar rcD ../../aos-hello-xz.deb debian-binary control.tar.xz data.tar.xz

$(BUILD_DIR)/aos.iso: $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/initrd.img $(BUILD_DIR)/fat32.img $(BUILD_DIR)/ext4.img $(BUILD_DIR)/aosfs.img $(BUILD_DIR)/install-boot.img $(BUILD_DIR)/install-bios.img
	@mkdir -p $(ISO_DIR)/boot/grub/i386-pc
	cp $(BUILD_DIR)/kernel.bin $(ISO_DIR)/boot/kernel.bin
	cp $(BUILD_DIR)/initrd.img $(ISO_DIR)/boot/initrd.img
	cp $(BUILD_DIR)/fat32.img $(ISO_DIR)/boot/fat32.img
	cp $(BUILD_DIR)/ext4.img $(ISO_DIR)/boot/ext4.img
	cp $(BUILD_DIR)/aosfs.img $(ISO_DIR)/boot/aosfs.img
	cp $(BUILD_DIR)/install-boot.img $(ISO_DIR)/boot/install-boot.img
	cp $(BUILD_DIR)/install-bios.img $(ISO_DIR)/boot/install-bios.img
	cp scripts/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	cp -r $(GRUB_I386_DIR)/. $(ISO_DIR)/boot/grub/i386-pc/
	grub-mkimage -d $(GRUB_I386_DIR) -O i386-pc -o $(ISO_DIR)/boot/grub/i386-pc/core.img -p /boot/grub iso9660 biosdisk multiboot2 normal configfile
	xorriso -as mkisofs -R -b boot/grub/i386-pc/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -o $(BUILD_DIR)/aos.iso $(ISO_DIR)

$(BUILD_DIR)/install-boot.img: $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/initrd.img scripts/grub-installed.cfg
	@mkdir -p $(BUILD_DIR)/install_boot_root/EFI/BOOT $(BUILD_DIR)/install_boot_root/boot/grub
	grub-mkstandalone -d $(GRUB_X86_64_EFI_DIR) -O x86_64-efi --modules="part_msdos fat multiboot2 normal configfile search search_fs_file efi_gop video video_fb" --locales="" --fonts="" -o $(BUILD_DIR)/install_boot_root/EFI/BOOT/BOOTX64.EFI "boot/grub/grub.cfg=scripts/grub-installed.cfg"
	cp $(BUILD_DIR)/kernel.bin $(BUILD_DIR)/install_boot_root/boot/kernel.bin
	cp $(BUILD_DIR)/initrd.img $(BUILD_DIR)/install_boot_root/boot/initrd.img
	cp scripts/grub-installed.cfg $(BUILD_DIR)/install_boot_root/boot/grub/grub.cfg
	dd if=/dev/zero of=$@ bs=1M count=16 status=none
	mkfs.fat -F 16 -n AOSBOOT $@
	mmd -i $@ ::/EFI ::/EFI/BOOT ::/boot ::/boot/grub
	mcopy -i $@ $(BUILD_DIR)/install_boot_root/EFI/BOOT/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $@ $(BUILD_DIR)/install_boot_root/boot/kernel.bin ::/boot/kernel.bin
	mcopy -i $@ $(BUILD_DIR)/install_boot_root/boot/initrd.img ::/boot/initrd.img
	mcopy -i $@ $(BUILD_DIR)/install_boot_root/boot/grub/grub.cfg ::/boot/grub/grub.cfg

$(BUILD_DIR)/install-bios.img: scripts/grub-installed.cfg
	@mkdir -p $(BUILD_DIR)/install_bios
	grub-mkimage -d $(GRUB_I386_DIR) -O i386-pc -o $(BUILD_DIR)/install_bios/core.img -p '(hd0,msdos1)/boot/grub' biosdisk part_msdos fat multiboot2 normal configfile search search_fs_file
	test $$(stat -c %s $(BUILD_DIR)/install_bios/core.img) -le 1048064
	dd if=/dev/zero of=$@ bs=1M count=1 status=none
	dd if=$(GRUB_I386_DIR)/boot.img of=$@ bs=512 count=1 conv=notrunc status=none
	dd if=$(BUILD_DIR)/install_bios/core.img of=$@ bs=512 seek=1 conv=notrunc status=none

$(BUILD_DIR)/initrd.img: hello.txt pakages/pakages.txt firmware/aos-wifi-placeholder.fw firmware/iwlwifi-test.fw $(LINUX_LD_SO) $(LINUX_LIBC_SO) $(BUILD_DIR)/linux-dynamic-hello $(BUILD_DIR)/aos-hello.deb $(BUILD_DIR)/aos-hello-xz.deb $(BUILD_DIR)/user.elf $(BUILD_DIR)/user2.elf $(BUILD_DIR)/shell.elf $(BUILD_DIR)/desktop.elf $(BUILD_DIR)/filetest.elf $(BUILD_DIR)/accesstest.elf $(BUILD_DIR)/openflagstest.elf $(BUILD_DIR)/duptest.elf $(BUILD_DIR)/pipetest.elf $(BUILD_DIR)/wait4test.elf $(BUILD_DIR)/stdincat.elf $(BUILD_DIR)/argvtest.elf $(BUILD_DIR)/pathtest.elf $(BUILD_DIR)/partitions.elf $(BUILD_DIR)/mounts.elf $(BUILD_DIR)/lspci.elf $(BUILD_DIR)/drivers.elf $(BUILD_DIR)/net.elf $(BUILD_DIR)/ip.elf $(BUILD_DIR)/wifi.elf $(BUILD_DIR)/bluetooth.elf $(BUILD_DIR)/firmware.elf $(BUILD_DIR)/usb.elf $(BUILD_DIR)/ping.elf $(BUILD_DIR)/ping6.elf $(BUILD_DIR)/rdisc6.elf $(BUILD_DIR)/route.elf $(BUILD_DIR)/neigh.elf $(BUILD_DIR)/dhcp.elf $(BUILD_DIR)/tcp.elf $(BUILD_DIR)/curl.elf $(BUILD_DIR)/acur.elf $(BUILD_DIR)/uni.elf $(BUILD_DIR)/afetch.elf $(BUILD_DIR)/kshttpget.elf $(BUILD_DIR)/tcpstress.elf $(BUILD_DIR)/dlstress.elf $(BUILD_DIR)/wget.elf $(BUILD_DIR)/https.elf $(BUILD_DIR)/sockclose.elf $(BUILD_DIR)/gethost.elf $(BUILD_DIR)/netcache.elf $(BUILD_DIR)/netstat.elf $(BUILD_DIR)/netrawtest.elf $(BUILD_DIR)/mem.elf $(BUILD_DIR)/uptime.elf $(BUILD_DIR)/uname.elf $(BUILD_DIR)/whoami.elf $(BUILD_DIR)/id.elf $(BUILD_DIR)/ps.elf $(BUILD_DIR)/cp.elf $(BUILD_DIR)/mv.elf $(BUILD_DIR)/echo.elf $(BUILD_DIR)/stat.elf $(BUILD_DIR)/cat.elf $(BUILD_DIR)/head.elf $(BUILD_DIR)/tail.elf $(BUILD_DIR)/true.elf $(BUILD_DIR)/false.elf $(BUILD_DIR)/asheel.elf $(BUILD_DIR)/asence.elf $(BUILD_DIR)/mui.elf $(BUILD_DIR)/aossetup.elf $(BUILD_DIR)/arootinstall.elf $(BUILD_DIR)/display.elf $(BUILD_DIR)/settings.elf $(BUILD_DIR)/vsys.elf $(BUILD_DIR)/shutdown.elf $(BUILD_DIR)/restart.elf $(BUILD_DIR)/date.elf $(BUILD_DIR)/touch.elf $(BUILD_DIR)/rm.elf $(BUILD_DIR)/mkdir.elf $(BUILD_DIR)/sudo.elf $(BUILD_DIR)/devtest.elf $(BUILD_DIR)/gfxdemo.elf $(BUILD_DIR)/inputtest.elf $(BUILD_DIR)/nano.elf
	rm -rf $(BUILD_DIR)/initrd_root
	@mkdir -p $(BUILD_DIR)/initrd_root
	@mkdir -p $(BUILD_DIR)/initrd_root/firmware
	@mkdir -p $(BUILD_DIR)/initrd_root/pakages
	@mkdir -p $(BUILD_DIR)/initrd_root/lib64
	@mkdir -p $(BUILD_DIR)/initrd_root/lib/x86_64-linux-gnu
	cp hello.txt $(BUILD_DIR)/initrd_root/hello.txt
	cp pakages/pakages.txt $(BUILD_DIR)/initrd_root/pakages/pakages.txt
	cp pakages/pakages.txt $(BUILD_DIR)/initrd_root/pakages.txt
	cp $(BUILD_DIR)/aos-hello-xz.deb $(BUILD_DIR)/initrd_root/aos-hello.deb
	cp $(BUILD_DIR)/aos-hello-xz.deb $(BUILD_DIR)/initrd_root/pakages/aos-hello.deb
	cp firmware/aos-wifi-placeholder.fw $(BUILD_DIR)/initrd_root/firmware/aos-wifi-placeholder.fw
	cp firmware/iwlwifi-test.fw $(BUILD_DIR)/initrd_root/firmware/iwlwifi-test.fw
	cp $(BUILD_DIR)/linux-dynamic-hello $(BUILD_DIR)/initrd_root/linux-dynamic-hello
	cp $(LINUX_LD_SO) $(BUILD_DIR)/initrd_root/lib64/ld-linux-x86-64.so.2
	cp $(LINUX_LD_SO) $(BUILD_DIR)/initrd_root/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
	cp $(LINUX_LIBC_SO) $(BUILD_DIR)/initrd_root/lib/x86_64-linux-gnu/libc.so.6
	cp $(BUILD_DIR)/user.elf $(BUILD_DIR)/initrd_root/user.elf
	cp $(BUILD_DIR)/user2.elf $(BUILD_DIR)/initrd_root/user2.elf
	cp $(BUILD_DIR)/shell.elf $(BUILD_DIR)/initrd_root/shell.elf
	cp $(BUILD_DIR)/shell.elf $(BUILD_DIR)/initrd_root/vash
	cp $(BUILD_DIR)/desktop.elf $(BUILD_DIR)/initrd_root/desktop.elf
	cp $(BUILD_DIR)/desktop.elf $(BUILD_DIR)/initrd_root/desktop
	cp $(BUILD_DIR)/desktop.elf $(BUILD_DIR)/initrd_root/dextop
	cp $(BUILD_DIR)/desktop.elf $(BUILD_DIR)/initrd_root/dm
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
	cp $(BUILD_DIR)/mounts.elf $(BUILD_DIR)/initrd_root/mounts.elf
	cp $(BUILD_DIR)/mounts.elf $(BUILD_DIR)/initrd_root/mounts
	cp $(BUILD_DIR)/lspci.elf $(BUILD_DIR)/initrd_root/lspci.elf
	cp $(BUILD_DIR)/lspci.elf $(BUILD_DIR)/initrd_root/lspci
	cp $(BUILD_DIR)/drivers.elf $(BUILD_DIR)/initrd_root/drivers.elf
	cp $(BUILD_DIR)/drivers.elf $(BUILD_DIR)/initrd_root/drivers
	cp $(BUILD_DIR)/drivers.elf $(BUILD_DIR)/initrd_root/driver
	cp $(BUILD_DIR)/net.elf $(BUILD_DIR)/initrd_root/net.elf
	cp $(BUILD_DIR)/net.elf $(BUILD_DIR)/initrd_root/net
	cp $(BUILD_DIR)/net.elf $(BUILD_DIR)/initrd_root/ifconfig
	cp $(BUILD_DIR)/ip.elf $(BUILD_DIR)/initrd_root/ip.elf
	cp $(BUILD_DIR)/ip.elf $(BUILD_DIR)/initrd_root/ip
	cp $(BUILD_DIR)/wifi.elf $(BUILD_DIR)/initrd_root/wifi.elf
	cp $(BUILD_DIR)/wifi.elf $(BUILD_DIR)/initrd_root/wifi
	cp $(BUILD_DIR)/bluetooth.elf $(BUILD_DIR)/initrd_root/bluetooth.elf
	cp $(BUILD_DIR)/bluetooth.elf $(BUILD_DIR)/initrd_root/bluetooth
	cp $(BUILD_DIR)/bluetooth.elf $(BUILD_DIR)/initrd_root/bt
	cp $(BUILD_DIR)/bluetooth.elf $(BUILD_DIR)/initrd_root/bluethoot-NOW
	cp $(BUILD_DIR)/bluetooth.elf $(BUILD_DIR)/initrd_root/bluetooth-NOW
	cp $(BUILD_DIR)/bluetooth.elf $(BUILD_DIR)/initrd_root/bluethoot-now
	cp $(BUILD_DIR)/bluetooth.elf $(BUILD_DIR)/initrd_root/bluetooth-now
	cp $(BUILD_DIR)/firmware.elf $(BUILD_DIR)/initrd_root/firmware.elf
	cp $(BUILD_DIR)/firmware.elf $(BUILD_DIR)/initrd_root/fw
	cp $(BUILD_DIR)/usb.elf $(BUILD_DIR)/initrd_root/usb.elf
	cp $(BUILD_DIR)/usb.elf $(BUILD_DIR)/initrd_root/usb
	cp $(BUILD_DIR)/ping.elf $(BUILD_DIR)/initrd_root/ping.elf
	cp $(BUILD_DIR)/ping.elf $(BUILD_DIR)/initrd_root/ping
	cp $(BUILD_DIR)/gethost.elf $(BUILD_DIR)/initrd_root/gethost.elf
	cp $(BUILD_DIR)/gethost.elf $(BUILD_DIR)/initrd_root/gethost
	cp $(BUILD_DIR)/gethost.elf $(BUILD_DIR)/initrd_root/dns
	cp $(BUILD_DIR)/gethost.elf $(BUILD_DIR)/initrd_root/nslookup
	cp $(BUILD_DIR)/netcache.elf $(BUILD_DIR)/initrd_root/netcache.elf
	cp $(BUILD_DIR)/netcache.elf $(BUILD_DIR)/initrd_root/netcache
	cp $(BUILD_DIR)/netstat.elf $(BUILD_DIR)/initrd_root/netstat.elf
	cp $(BUILD_DIR)/netstat.elf $(BUILD_DIR)/initrd_root/netstat
	cp $(BUILD_DIR)/ping6.elf $(BUILD_DIR)/initrd_root/ping6.elf
	cp $(BUILD_DIR)/ping6.elf $(BUILD_DIR)/initrd_root/ping6
	cp $(BUILD_DIR)/rdisc6.elf $(BUILD_DIR)/initrd_root/rdisc6.elf
	cp $(BUILD_DIR)/rdisc6.elf $(BUILD_DIR)/initrd_root/rdisc6
	cp $(BUILD_DIR)/route.elf $(BUILD_DIR)/initrd_root/route.elf
	cp $(BUILD_DIR)/route.elf $(BUILD_DIR)/initrd_root/route
	cp $(BUILD_DIR)/neigh.elf $(BUILD_DIR)/initrd_root/neigh.elf
	cp $(BUILD_DIR)/neigh.elf $(BUILD_DIR)/initrd_root/neigh
	cp $(BUILD_DIR)/dhcp.elf $(BUILD_DIR)/initrd_root/dhcp.elf
	cp $(BUILD_DIR)/dhcp.elf $(BUILD_DIR)/initrd_root/dhcp
	cp $(BUILD_DIR)/tcp.elf $(BUILD_DIR)/initrd_root/tcp.elf
	cp $(BUILD_DIR)/tcp.elf $(BUILD_DIR)/initrd_root/tcp
	cp $(BUILD_DIR)/tcp.elf $(BUILD_DIR)/initrd_root/httpget.elf
	cp $(BUILD_DIR)/tcp.elf $(BUILD_DIR)/initrd_root/httpget
	cp $(BUILD_DIR)/curl.elf $(BUILD_DIR)/initrd_root/curl.elf
	cp $(BUILD_DIR)/curl.elf $(BUILD_DIR)/initrd_root/curl
	cp $(BUILD_DIR)/acur.elf $(BUILD_DIR)/initrd_root/acur.elf
	cp $(BUILD_DIR)/acur.elf $(BUILD_DIR)/initrd_root/acur
	cp $(BUILD_DIR)/uni.elf $(BUILD_DIR)/initrd_root/uni.elf
	cp $(BUILD_DIR)/uni.elf $(BUILD_DIR)/initrd_root/uni
	cp $(BUILD_DIR)/afetch.elf $(BUILD_DIR)/initrd_root/afetch.elf
	cp $(BUILD_DIR)/afetch.elf $(BUILD_DIR)/initrd_root/afetch
	cp $(BUILD_DIR)/kshttpget.elf $(BUILD_DIR)/initrd_root/kshttpget.elf
	cp $(BUILD_DIR)/kshttpget.elf $(BUILD_DIR)/initrd_root/kshttpget
	cp $(BUILD_DIR)/tcpstress.elf $(BUILD_DIR)/initrd_root/tcpstress.elf
	cp $(BUILD_DIR)/tcpstress.elf $(BUILD_DIR)/initrd_root/tcpstress
	cp $(BUILD_DIR)/dlstress.elf $(BUILD_DIR)/initrd_root/dlstress.elf
	cp $(BUILD_DIR)/dlstress.elf $(BUILD_DIR)/initrd_root/dlstress
	cp $(BUILD_DIR)/wget.elf $(BUILD_DIR)/initrd_root/wget.elf
	cp $(BUILD_DIR)/wget.elf $(BUILD_DIR)/initrd_root/wget
	cp $(BUILD_DIR)/wget.elf $(BUILD_DIR)/initrd_root/download
	cp $(BUILD_DIR)/https.elf $(BUILD_DIR)/initrd_root/https.elf
	cp $(BUILD_DIR)/https.elf $(BUILD_DIR)/initrd_root/https
	cp $(BUILD_DIR)/https.elf $(BUILD_DIR)/initrd_root/tlsprobe
	cp $(BUILD_DIR)/sockclose.elf $(BUILD_DIR)/initrd_root/sockclose.elf
	cp $(BUILD_DIR)/sockclose.elf $(BUILD_DIR)/initrd_root/sockclose
	cp $(BUILD_DIR)/netrawtest.elf $(BUILD_DIR)/initrd_root/netrawtest.elf
	cp $(BUILD_DIR)/netrawtest.elf $(BUILD_DIR)/initrd_root/netrawtest
	cp $(BUILD_DIR)/mem.elf $(BUILD_DIR)/initrd_root/mem.elf
	cp $(BUILD_DIR)/mem.elf $(BUILD_DIR)/initrd_root/mem
	cp $(BUILD_DIR)/uptime.elf $(BUILD_DIR)/initrd_root/uptime.elf
	cp $(BUILD_DIR)/uptime.elf $(BUILD_DIR)/initrd_root/uptime
	cp $(BUILD_DIR)/uname.elf $(BUILD_DIR)/initrd_root/uname.elf
	cp $(BUILD_DIR)/uname.elf $(BUILD_DIR)/initrd_root/uname
	cp $(BUILD_DIR)/whoami.elf $(BUILD_DIR)/initrd_root/whoami.elf
	cp $(BUILD_DIR)/whoami.elf $(BUILD_DIR)/initrd_root/whoami
	cp $(BUILD_DIR)/id.elf $(BUILD_DIR)/initrd_root/id.elf
	cp $(BUILD_DIR)/id.elf $(BUILD_DIR)/initrd_root/id
	cp $(BUILD_DIR)/ps.elf $(BUILD_DIR)/initrd_root/ps.elf
	cp $(BUILD_DIR)/ps.elf $(BUILD_DIR)/initrd_root/ps
	cp $(BUILD_DIR)/cp.elf $(BUILD_DIR)/initrd_root/cp.elf
	cp $(BUILD_DIR)/cp.elf $(BUILD_DIR)/initrd_root/cp
	cp $(BUILD_DIR)/mv.elf $(BUILD_DIR)/initrd_root/mv.elf
	cp $(BUILD_DIR)/mv.elf $(BUILD_DIR)/initrd_root/mv
	cp $(BUILD_DIR)/echo.elf $(BUILD_DIR)/initrd_root/echo.elf
	cp $(BUILD_DIR)/echo.elf $(BUILD_DIR)/initrd_root/echo
	cp $(BUILD_DIR)/stat.elf $(BUILD_DIR)/initrd_root/stat.elf
	cp $(BUILD_DIR)/stat.elf $(BUILD_DIR)/initrd_root/stat
	cp $(BUILD_DIR)/cat.elf $(BUILD_DIR)/initrd_root/cat.elf
	cp $(BUILD_DIR)/cat.elf $(BUILD_DIR)/initrd_root/cat
	cp $(BUILD_DIR)/head.elf $(BUILD_DIR)/initrd_root/head.elf
	cp $(BUILD_DIR)/head.elf $(BUILD_DIR)/initrd_root/head
	cp $(BUILD_DIR)/tail.elf $(BUILD_DIR)/initrd_root/tail.elf
	cp $(BUILD_DIR)/tail.elf $(BUILD_DIR)/initrd_root/tail
	cp $(BUILD_DIR)/true.elf $(BUILD_DIR)/initrd_root/true.elf
	cp $(BUILD_DIR)/true.elf $(BUILD_DIR)/initrd_root/true
	cp $(BUILD_DIR)/false.elf $(BUILD_DIR)/initrd_root/false.elf
	cp $(BUILD_DIR)/false.elf $(BUILD_DIR)/initrd_root/false
	cp $(BUILD_DIR)/asence.elf $(BUILD_DIR)/initrd_root/asence.elf
	for alias in $(ASENCE_ALIASES); do cp $(BUILD_DIR)/asence.elf "$(BUILD_DIR)/initrd_root/$$alias"; done
	cp $(BUILD_DIR)/mui.elf $(BUILD_DIR)/initrd_root/mui.elf
	cp $(BUILD_DIR)/mui.elf $(BUILD_DIR)/initrd_root/mui
	cp $(BUILD_DIR)/mui.elf $(BUILD_DIR)/initrd_root/MUI
	cp $(BUILD_DIR)/aossetup.elf $(BUILD_DIR)/initrd_root/aossetup.elf
	cp $(BUILD_DIR)/aossetup.elf $(BUILD_DIR)/initrd_root/aossetup
	cp $(BUILD_DIR)/arootinstall.elf $(BUILD_DIR)/initrd_root/arootinstall.elf
	cp $(BUILD_DIR)/arootinstall.elf $(BUILD_DIR)/initrd_root/arootinstall
	cp $(BUILD_DIR)/arootinstall.elf $(BUILD_DIR)/initrd_root/Arootinstall
	cp $(BUILD_DIR)/arootinstall.elf $(BUILD_DIR)/initrd_root/install-aos
	cp $(BUILD_DIR)/display.elf $(BUILD_DIR)/initrd_root/display.elf
	cp $(BUILD_DIR)/display.elf $(BUILD_DIR)/initrd_root/display
	cp $(BUILD_DIR)/settings.elf $(BUILD_DIR)/initrd_root/settings.elf
	cp $(BUILD_DIR)/settings.elf $(BUILD_DIR)/initrd_root/settings
	cp $(BUILD_DIR)/vsys.elf $(BUILD_DIR)/initrd_root/vsys.elf
	cp $(BUILD_DIR)/vsys.elf $(BUILD_DIR)/initrd_root/vsys
	cp $(BUILD_DIR)/vsys.elf $(BUILD_DIR)/initrd_root/Vsys
	cp $(BUILD_DIR)/shutdown.elf $(BUILD_DIR)/initrd_root/shutdown.elf
	cp $(BUILD_DIR)/shutdown.elf $(BUILD_DIR)/initrd_root/shutdown
	cp $(BUILD_DIR)/restart.elf $(BUILD_DIR)/initrd_root/restart.elf
	cp $(BUILD_DIR)/restart.elf $(BUILD_DIR)/initrd_root/restart
	cp $(BUILD_DIR)/restart.elf $(BUILD_DIR)/initrd_root/reboot
	cp $(BUILD_DIR)/date.elf $(BUILD_DIR)/initrd_root/date.elf
	cp $(BUILD_DIR)/date.elf $(BUILD_DIR)/initrd_root/date
	cp $(BUILD_DIR)/touch.elf $(BUILD_DIR)/initrd_root/touch.elf
	cp $(BUILD_DIR)/touch.elf $(BUILD_DIR)/initrd_root/touch
	cp $(BUILD_DIR)/rm.elf $(BUILD_DIR)/initrd_root/rm.elf
	cp $(BUILD_DIR)/rm.elf $(BUILD_DIR)/initrd_root/rm
	cp $(BUILD_DIR)/mkdir.elf $(BUILD_DIR)/initrd_root/mkdir.elf
	cp $(BUILD_DIR)/mkdir.elf $(BUILD_DIR)/initrd_root/mkdir
	cp $(BUILD_DIR)/sudo.elf $(BUILD_DIR)/initrd_root/sudo.elf
	cp $(BUILD_DIR)/sudo.elf $(BUILD_DIR)/initrd_root/sudo
	cp $(BUILD_DIR)/devtest.elf $(BUILD_DIR)/initrd_root/devtest.elf
	cp $(BUILD_DIR)/devtest.elf $(BUILD_DIR)/initrd_root/devtest
	cp $(BUILD_DIR)/gfxdemo.elf $(BUILD_DIR)/initrd_root/gfxdemo.elf
	cp $(BUILD_DIR)/gfxdemo.elf $(BUILD_DIR)/initrd_root/gfxdemo
	cp $(BUILD_DIR)/inputtest.elf $(BUILD_DIR)/initrd_root/inputtest.elf
	cp $(BUILD_DIR)/inputtest.elf $(BUILD_DIR)/initrd_root/inputtest
	for alias in $(PARTITION_ALIASES); do cp $(BUILD_DIR)/partitions.elf "$(BUILD_DIR)/initrd_root/$$alias"; done
	cp $(BUILD_DIR)/nano.elf $(BUILD_DIR)/initrd_root/aosnano.elf
	cp $(BUILD_DIR)/nano.elf $(BUILD_DIR)/initrd_root/aosnano
	cp $(BUILD_DIR)/nano.elf $(BUILD_DIR)/initrd_root/nano
	cp $(BUILD_DIR)/asheel.elf $(BUILD_DIR)/initrd_root/asheel.elf
	cp $(BUILD_DIR)/asheel.elf $(BUILD_DIR)/initrd_root/asheel
	cp $(BUILD_DIR)/asheel.elf $(BUILD_DIR)/initrd_root/Asheel
	for alias in $(ASHEEL_ALIASES); do cp $(BUILD_DIR)/asheel.elf "$(BUILD_DIR)/initrd_root/$$alias"; done
	cd $(BUILD_DIR)/initrd_root && { printf "hello.txt\npakages/pakages.txt\npakages.txt\naos-hello.deb\npakages/aos-hello.deb\nfirmware/aos-wifi-placeholder.fw\nfirmware/iwlwifi-test.fw\nlinux-dynamic-hello\nlib64\nlib64/ld-linux-x86-64.so.2\nlib\nlib/x86_64-linux-gnu\nlib/x86_64-linux-gnu/ld-linux-x86-64.so.2\nlib/x86_64-linux-gnu/libc.so.6\nuser.elf\nuser2.elf\nshell.elf\nvash\ndesktop.elf\ndesktop\ndextop\ndm\nfiletest.elf\naccesstest.elf\nduptest.elf\npipetest.elf\nwait4test.elf\nstdincat.elf\nargvtest.elf\npathtest.elf\npartitions.elf\npartitions\nmounts.elf\nmounts\nlspci.elf\nlspci\ndrivers.elf\ndrivers\ndriver\nnet.elf\nnet\nifconfig\nip.elf\nip\nwifi.elf\nwifi\nbluetooth.elf\nbluetooth\nbt\nbluethoot-NOW\nbluetooth-NOW\nbluethoot-now\nbluetooth-now\nfirmware.elf\nfw\nusb.elf\nusb\nping.elf\nping\ndns\ngethost.elf\ngethost\nnslookup\nnetcache.elf\nnetcache\nnetstat.elf\nnetstat\nping6.elf\nping6\nrdisc6.elf\nrdisc6\nroute.elf\nroute\nneigh.elf\nneigh\ndhcp.elf\ndhcp\ntcp.elf\ntcp\nhttpget.elf\nhttpget\ncurl.elf\ncurl\nacur.elf\nacur\nuni.elf\nuni\nafetch.elf\nafetch\nkshttpget.elf\nkshttpget\ntcpstress.elf\ntcpstress\ndlstress.elf\ndlstress\nwget.elf\nwget\ndownload\nhttps.elf\nhttps\ntlsprobe\nsockclose.elf\nsockclose\nnetrawtest.elf\nnetrawtest\nmem.elf\nmem\nuptime.elf\nuptime\nuname.elf\nuname\nwhoami.elf\nwhoami\nid.elf\nid\nps.elf\nps\ncp.elf\ncp\nmv.elf\nmv\necho.elf\necho\nstat.elf\nstat\ncat.elf\ncat\nhead.elf\nhead\ntail.elf\ntail\ntrue.elf\ntrue\nfalse.elf\nfalse\nasheel.elf\nasheel\nAsheel\nasence.elf\nmui.elf\nmui\nMUI\naossetup.elf\naossetup\narootinstall.elf\narootinstall\nArootinstall\ninstall-aos\ndisplay.elf\ndisplay\nsettings.elf\nsettings\nvsys.elf\nvsys\nVsys\nshutdown.elf\nshutdown\nrestart.elf\nrestart\nreboot\ndate.elf\ndate\ntouch.elf\ntouch\nrm.elf\nrm\nmkdir.elf\nmkdir\nsudo.elf\nsudo\ndevtest.elf\ndevtest\ngfxdemo.elf\ngfxdemo\ninputtest.elf\ninputtest\n"; for alias in $(PARTITION_ALIASES); do printf "%s\n" "$$alias"; done; for alias in $(ASHEEL_ALIASES); do printf "%s\n" "$$alias"; done; for alias in $(ASENCE_ALIASES); do printf "%s\n" "$$alias"; done; printf "aosnano.elf\naosnano\nnano\n"; } | cpio -o -H newc > ../initrd.img

$(BUILD_DIR)/fat32.img: hello.txt
	@mkdir -p $(BUILD_DIR)/fat32_root
	cp hello.txt $(BUILD_DIR)/fat32_root/HELLO.TXT
	printf "AOS FAT32 test image\n" > $(BUILD_DIR)/fat32_root/README.TXT
	dd if=/dev/zero of=$@ bs=1M count=16 status=none
	mkfs.fat -F 32 -n AOSFAT32 $@
	mcopy -i $@ $(BUILD_DIR)/fat32_root/HELLO.TXT ::HELLO.TXT
	mcopy -i $@ $(BUILD_DIR)/fat32_root/README.TXT ::README.TXT

$(BUILD_DIR)/ext4.img: hello.txt partitions/structure.txt
	@mkdir -p $(BUILD_DIR)/ext4_root
	mkdir -p $(BUILD_DIR)/ext4_root/Desktop $(BUILD_DIR)/ext4_root/Music $(BUILD_DIR)/ext4_root/Photos $(BUILD_DIR)/ext4_root/Video $(BUILD_DIR)/ext4_root/MUI $(BUILD_DIR)/ext4_root/Starred $(BUILD_DIR)/ext4_root/Trash
	cp hello.txt $(BUILD_DIR)/ext4_root/hello.txt
	printf "AOS EXT4 test image\n" > $(BUILD_DIR)/ext4_root/readme.txt
	dd if=/dev/zero of=$@ bs=1M count=16 status=none
	mkfs.ext4 -q -F -L AOSEXT4 -O ^has_journal,^metadata_csum,^64bit,^dir_index -d $(BUILD_DIR)/ext4_root $@

$(BUILD_DIR)/aosfs.img:
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$@ bs=1M count=$(AOSFS_IMAGE_SIZE_MB) status=none
	printf 'AOSFS1' | dd of=$@ bs=1 seek=0 conv=notrunc status=none

$(BUILD_DIR)/live-disk.img:
	@mkdir -p $(BUILD_DIR)
	dd if=/dev/zero of=$@ bs=1M count=$(AOS_LIVE_DISK_SIZE_MB) status=none
	printf 'AOSFS1' | dd of=$@ bs=1 seek=0 conv=notrunc status=none

$(BUILD_DIR)/user.o: userspace/user.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/user.elf: $(BUILD_DIR)/user.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/user.o

$(BUILD_DIR)/shell.o: userspace/shell.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/shell.elf: $(BUILD_DIR)/shell.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/shell.o

$(BUILD_DIR)/desktop.o: userspace/desktop.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/desktop.elf: $(BUILD_DIR)/desktop.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/desktop.o

$(BUILD_DIR)/user2.o: userspace/user2.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/user2.elf: $(BUILD_DIR)/user2.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/user2.o

$(BUILD_DIR)/filetest.o: userspace/filetest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/filetest.elf: $(BUILD_DIR)/filetest.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/filetest.o

$(BUILD_DIR)/accesstest.o: userspace/accesstest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/accesstest.elf: $(BUILD_DIR)/accesstest.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/accesstest.o

$(BUILD_DIR)/openflagstest.o: userspace/openflagstest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/openflagstest.elf: $(BUILD_DIR)/openflagstest.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/openflagstest.o

$(BUILD_DIR)/duptest.o: userspace/duptest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/duptest.elf: $(BUILD_DIR)/duptest.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/duptest.o

$(BUILD_DIR)/pipetest.o: userspace/pipetest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/pipetest.elf: $(BUILD_DIR)/pipetest.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/pipetest.o

$(BUILD_DIR)/wait4test.o: userspace/wait4test.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/wait4test.elf: $(BUILD_DIR)/wait4test.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/wait4test.o

$(BUILD_DIR)/stdincat.o: userspace/stdincat.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/stdincat.elf: $(BUILD_DIR)/stdincat.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/stdincat.o

$(BUILD_DIR)/argvtest.o: userspace/argvtest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/argvtest.elf: $(BUILD_DIR)/argvtest.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/argvtest.o

$(BUILD_DIR)/pathtest.o: userspace/pathtest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/pathtest.elf: $(BUILD_DIR)/pathtest.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/pathtest.o

$(BUILD_DIR)/partitions.o: userspace/partitions.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/partitions.elf: $(BUILD_DIR)/partitions.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/partitions.o

$(BUILD_DIR)/mounts.o: userspace/mounts.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/mounts.elf: $(BUILD_DIR)/mounts.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/mounts.o

$(BUILD_DIR)/lspci.o: userspace/lspci.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/lspci.elf: $(BUILD_DIR)/lspci.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/lspci.o

$(BUILD_DIR)/drivers.o: userspace/drivers.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/drivers.elf: $(BUILD_DIR)/drivers.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/drivers.o

$(BUILD_DIR)/net.o: userspace/net.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/net.elf: $(BUILD_DIR)/net.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/net.o

$(BUILD_DIR)/wifi.o: userspace/wifi.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/wifi.elf: $(BUILD_DIR)/wifi.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/wifi.o

$(BUILD_DIR)/bluetooth_cmd.o: userspace/bluetooth.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/bluetooth.elf: $(BUILD_DIR)/bluetooth_cmd.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/bluetooth_cmd.o

$(BUILD_DIR)/firmware_cmd.o: userspace/firmware.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/firmware.elf: $(BUILD_DIR)/firmware_cmd.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/firmware_cmd.o

$(BUILD_DIR)/usb.o: userspace/usb.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/usb.elf: $(BUILD_DIR)/usb.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/usb.o

$(BUILD_DIR)/ping.o: userspace/ping.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/ping.elf: $(BUILD_DIR)/ping.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/ping.o

$(BUILD_DIR)/ping6.o: userspace/ping6.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/ping6.elf: $(BUILD_DIR)/ping6.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/ping6.o

$(BUILD_DIR)/rdisc6.o: userspace/rdisc6.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/rdisc6.elf: $(BUILD_DIR)/rdisc6.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/rdisc6.o

$(BUILD_DIR)/route.o: userspace/route.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/route.elf: $(BUILD_DIR)/route.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/route.o

$(BUILD_DIR)/neigh.o: userspace/neigh.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/neigh.elf: $(BUILD_DIR)/neigh.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/neigh.o

$(BUILD_DIR)/dhcp.o: userspace/dhcp.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/dhcp.elf: $(BUILD_DIR)/dhcp.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/dhcp.o

$(BUILD_DIR)/tcp.o: userspace/tcp.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/tcp.elf: $(BUILD_DIR)/tcp.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/tcp.o

$(BUILD_DIR)/curl.o: userspace/curl.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/curl.elf: $(BUILD_DIR)/curl.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/curl.o

$(BUILD_DIR)/acur.o: userspace/acur.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/acur.elf: $(BUILD_DIR)/acur.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/acur.o

$(BUILD_DIR)/uni.o: userspace/uni.c userspace/lib/uni_inflate.h userspace/lib/uni_xz.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -Iuserspace -c $< -o $@

$(BUILD_DIR)/uni_inflate.o: userspace/lib/uni_inflate.c userspace/lib/uni_inflate.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -Iuserspace -c $< -o $@

$(BUILD_DIR)/uni_xz.o: userspace/lib/uni_xz.c userspace/lib/uni_xz.h $(wildcard userspace/lib/xz/*)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -Iuserspace -c $< -o $@

$(BUILD_DIR)/uni.elf: $(BUILD_DIR)/uni.o $(BUILD_DIR)/uni_inflate.o $(BUILD_DIR)/uni_xz.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/uni.o $(BUILD_DIR)/uni_inflate.o $(BUILD_DIR)/uni_xz.o

$(BUILD_DIR)/afetch.o: userspace/afetch.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/afetch.elf: $(BUILD_DIR)/afetch.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/afetch.o

$(BUILD_DIR)/kshttpget.o: userspace/kshttpget.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/kshttpget.elf: $(BUILD_DIR)/kshttpget.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/kshttpget.o

$(BUILD_DIR)/tcpstress.o: userspace/tcpstress.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/tcpstress.elf: $(BUILD_DIR)/tcpstress.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/tcpstress.o

$(BUILD_DIR)/dlstress.o: userspace/dlstress.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/dlstress.elf: $(BUILD_DIR)/dlstress.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/dlstress.o

$(BUILD_DIR)/wget.o: userspace/wget.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/wget.elf: $(BUILD_DIR)/wget.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/wget.o

$(BUILD_DIR)/https.o: userspace/https.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/https.elf: $(BUILD_DIR)/https.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/https.o

$(BUILD_DIR)/sockclose.o: userspace/sockclose.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/sockclose.elf: $(BUILD_DIR)/sockclose.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/sockclose.o

$(BUILD_DIR)/gethost.o: userspace/gethost.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/gethost.elf: $(BUILD_DIR)/gethost.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/gethost.o

$(BUILD_DIR)/netcache.o: userspace/netcache.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/netcache.elf: $(BUILD_DIR)/netcache.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/netcache.o

$(BUILD_DIR)/netstat.o: userspace/netstat.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/netstat.elf: $(BUILD_DIR)/netstat.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/netstat.o

$(BUILD_DIR)/ip.o: userspace/ip.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/ip.elf: $(BUILD_DIR)/ip.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/ip.o

$(BUILD_DIR)/netrawtest.o: userspace/netrawtest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/netrawtest.elf: $(BUILD_DIR)/netrawtest.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/netrawtest.o

$(BUILD_DIR)/mem.o: userspace/mem.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/mem.elf: $(BUILD_DIR)/mem.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/mem.o

$(BUILD_DIR)/uptime.o: userspace/uptime.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/uptime.elf: $(BUILD_DIR)/uptime.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/uptime.o

$(BUILD_DIR)/uname.o: userspace/uname.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/uname.elf: $(BUILD_DIR)/uname.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/uname.o

$(BUILD_DIR)/whoami.o: userspace/whoami.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/whoami.elf: $(BUILD_DIR)/whoami.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/whoami.o

$(BUILD_DIR)/id.o: userspace/id.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/id.elf: $(BUILD_DIR)/id.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/id.o

$(BUILD_DIR)/ps.o: userspace/ps.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/ps.elf: $(BUILD_DIR)/ps.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/ps.o

$(BUILD_DIR)/cp.o: userspace/cp.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/cp.elf: $(BUILD_DIR)/cp.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/cp.o

$(BUILD_DIR)/mv.o: userspace/mv.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/mv.elf: $(BUILD_DIR)/mv.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/mv.o

$(BUILD_DIR)/echo.o: userspace/echo.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/echo.elf: $(BUILD_DIR)/echo.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/echo.o

$(BUILD_DIR)/stat.o: userspace/stat.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/stat.elf: $(BUILD_DIR)/stat.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/stat.o

$(BUILD_DIR)/cat.o: userspace/cat.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/cat.elf: $(BUILD_DIR)/cat.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/cat.o

$(BUILD_DIR)/head.o: userspace/head.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/head.elf: $(BUILD_DIR)/head.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/head.o

$(BUILD_DIR)/tail.o: userspace/tail.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/tail.elf: $(BUILD_DIR)/tail.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/tail.o

$(BUILD_DIR)/true.o: userspace/true.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/true.elf: $(BUILD_DIR)/true.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/true.o

$(BUILD_DIR)/false.o: userspace/false.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/false.elf: $(BUILD_DIR)/false.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/false.o

$(BUILD_DIR)/asheel.o: userspace/asheel.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/asheel.elf: $(BUILD_DIR)/asheel.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/asheel.o

$(BUILD_DIR)/asence.o: userspace/asence.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/asence.elf: $(BUILD_DIR)/asence.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/asence.o

$(BUILD_DIR)/mui.o: userspace/mui.c userspace/mui_wallpaper.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/mui.elf: $(BUILD_DIR)/mui.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/mui.o

$(BUILD_DIR)/sudo.o: userspace/sudo.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/sudo.elf: $(BUILD_DIR)/sudo.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/sudo.o

$(BUILD_DIR)/devtest.o: userspace/devtest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/devtest.elf: $(BUILD_DIR)/devtest.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/devtest.o

$(BUILD_DIR)/aos_gfx.o: userspace/lib/aos_gfx.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/gfxdemo.o: userspace/gfxdemo.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/aos_input.o: userspace/lib/aos_input.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/gfxdemo.elf: $(BUILD_DIR)/gfxdemo.o $(BUILD_DIR)/aos_gfx.o $(BUILD_DIR)/aos_input.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/gfxdemo.o $(BUILD_DIR)/aos_gfx.o $(BUILD_DIR)/aos_input.o

$(BUILD_DIR)/inputtest.o: userspace/inputtest.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/inputtest.elf: $(BUILD_DIR)/inputtest.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/inputtest.o

$(BUILD_DIR)/aossetup.o: userspace/aossetup.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/aossetup.elf: $(BUILD_DIR)/aossetup.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/aossetup.o

$(BUILD_DIR)/arootinstall.o: userspace/arootinstall.c include/aos_install.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/arootinstall.elf: $(BUILD_DIR)/arootinstall.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/arootinstall.o

$(BUILD_DIR)/display.o: userspace/display.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/display.elf: $(BUILD_DIR)/display.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/display.o

$(BUILD_DIR)/settings.o: userspace/settings.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/settings.elf: $(BUILD_DIR)/settings.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/settings.o

$(BUILD_DIR)/vsys.o: userspace/vsys.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/vsys.elf: $(BUILD_DIR)/vsys.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/vsys.o

$(BUILD_DIR)/shutdown.o: userspace/shutdown.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/shutdown.elf: $(BUILD_DIR)/shutdown.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/shutdown.o

$(BUILD_DIR)/restart.o: userspace/restart.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/restart.elf: $(BUILD_DIR)/restart.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/restart.o

$(BUILD_DIR)/date.o: userspace/date.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/date.elf: $(BUILD_DIR)/date.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/date.o

$(BUILD_DIR)/touch.o: userspace/touch.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/touch.elf: $(BUILD_DIR)/touch.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/touch.o

$(BUILD_DIR)/rm.o: userspace/rm.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/rm.elf: $(BUILD_DIR)/rm.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/rm.o

$(BUILD_DIR)/mkdir.o: userspace/mkdir.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/mkdir.elf: $(BUILD_DIR)/mkdir.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/mkdir.o

$(BUILD_DIR)/nano.o: userspace/nano.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) -f elf64 $< -o $@

$(BUILD_DIR)/nano.elf: $(BUILD_DIR)/nano.o userspace/user.ld
	$(LD) $(USER_LDFLAGS) -T userspace/user.ld -o $@ $(BUILD_DIR)/nano.o

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

$(BUILD_DIR)/gfx.o: drivers/video/gfx.c
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

$(BUILD_DIR)/mouse.o: drivers/char/mouse.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_c.o: kernel/syscall.c kernel/syscall/syscall_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_ipc.o: kernel/syscall/ipc.c kernel/syscall/syscall_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_process_lifecycle.o: kernel/syscall/process_lifecycle.c kernel/syscall/syscall_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_memory.o: kernel/syscall/memory.c kernel/syscall/syscall_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_terminal_io.o: kernel/syscall/terminal_io.c kernel/syscall/syscall_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_fs.o: kernel/syscall/fs.c kernel/syscall/syscall_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_process_info.o: kernel/syscall/process_info.c kernel/syscall/syscall_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_network.o: kernel/syscall/network.c kernel/syscall/syscall_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_linux_misc.o: kernel/syscall/linux_misc.c kernel/syscall/syscall_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_aos_services.o: kernel/syscall/aos_services.c kernel/syscall/syscall_internal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall_dispatch.o: kernel/syscall/dispatch.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vfs.o: kernel/vfs.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/blkdev.o: kernel/blkdev.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pci.o: kernel/pci.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/driver.o: kernel/driver.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/input.o: kernel/input.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/firmware.o: kernel/firmware.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mac80211.o: kernel/mac80211.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/netdev.o: kernel/netdev.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ata.o: kernel/ata.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/e1000.o: drivers/net/e1000.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/wifi_driver.o: drivers/net/wifi.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/bluetooth.o: kernel/bluetooth.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/xhci.o: drivers/usb/xhci.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/aosfs.o: kernel/aosfs.c
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

$(BUILD_DIR)/thermal.o: kernel/thermal.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/credentials.o: kernel/credentials.c include/credentials.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/session.o: kernel/session.c include/session.h include/aos_session.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/installer.o: kernel/installer.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tty.o: kernel/tty.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/rtc.o: kernel/rtc.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Utility ---

clean:
	rm -rf $(BUILD_DIR)

run: all $(BUILD_DIR)/live-disk.img
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/aos.iso $(AOSFS_DRIVE_ARGS) $(AOS_NET_ARGS) $(AOS_USB_ARGS) -display gtk -serial stdio -d int -D qemu.log; stty sane; printf '\033[0m\033[?25h\n'

run-headless: all $(BUILD_DIR)/live-disk.img
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/aos.iso $(AOSFS_DRIVE_ARGS) $(AOS_NET_ARGS) $(AOS_USB_ARGS) -serial stdio -display none -d int -D qemu.log; stty sane; printf '\033[0m\033[?25h\n'

run-64m: all $(BUILD_DIR)/live-disk.img
	qemu-system-x86_64 -m 64M -cdrom $(BUILD_DIR)/aos.iso $(AOSFS_DRIVE_ARGS) $(AOS_NET_ARGS) $(AOS_USB_ARGS) -serial stdio -display none -d int -D qemu-64m.log; stty sane; printf '\033[0m\033[?25h\n'

run-32m: all $(BUILD_DIR)/live-disk.img
	qemu-system-x86_64 -m 32M -cdrom $(BUILD_DIR)/aos.iso $(AOSFS_DRIVE_ARGS) $(AOS_NET_ARGS) $(AOS_USB_ARGS) -serial stdio -display none -d int -D qemu-32m.log; stty sane; printf '\033[0m\033[?25h\n'

run-16m: all $(BUILD_DIR)/live-disk.img
	qemu-system-x86_64 -m 16M -cdrom $(BUILD_DIR)/aos.iso $(AOSFS_DRIVE_ARGS) $(AOS_NET_ARGS) $(AOS_USB_ARGS) -serial stdio -display none -d int -D qemu-16m.log; stty sane; printf '\033[0m\033[?25h\n'

run-curses: all $(BUILD_DIR)/live-disk.img
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/aos.iso $(AOSFS_DRIVE_ARGS) $(AOS_NET_ARGS) $(AOS_USB_ARGS) -display curses -serial none -d int -D qemu.log

UNI_CODEC_TEST_DIR = $(BUILD_DIR)/tests/uni-codecs

$(UNI_CODEC_TEST_DIR)/inflate-test: tests/uni_inflate_test.c userspace/lib/uni_inflate.c userspace/lib/uni_inflate.h
	@mkdir -p $(UNI_CODEC_TEST_DIR)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra tests/uni_inflate_test.c userspace/lib/uni_inflate.c -o $@

$(UNI_CODEC_TEST_DIR)/xz-test: tests/uni_xz_test.c userspace/lib/uni_xz.c userspace/lib/uni_xz.h $(wildcard userspace/lib/xz/*)
	@mkdir -p $(UNI_CODEC_TEST_DIR)
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra tests/uni_xz_test.c userspace/lib/uni_xz.c -o $@

$(UNI_CODEC_TEST_DIR)/payload.gz: README.md
	@mkdir -p $(UNI_CODEC_TEST_DIR)
	gzip -n -9 -c $< > $@

$(UNI_CODEC_TEST_DIR)/payload-crc64.xz: README.md
	@mkdir -p $(UNI_CODEC_TEST_DIR)
	xz -9 --threads=1 --check=crc64 -c $< > $@

$(UNI_CODEC_TEST_DIR)/payload-crc32.xz: README.md
	@mkdir -p $(UNI_CODEC_TEST_DIR)
	xz -9 --threads=1 --check=crc32 -c $< > $@

$(UNI_CODEC_TEST_DIR)/payload-none.xz: README.md
	@mkdir -p $(UNI_CODEC_TEST_DIR)
	xz -9 --threads=1 --check=none -c $< > $@

$(UNI_CODEC_TEST_DIR)/payload-x86.xz: README.md
	@mkdir -p $(UNI_CODEC_TEST_DIR)
	xz -9 --threads=1 --check=crc64 --x86 --lzma2 -c $< > $@

test-uni-codecs: $(UNI_CODEC_TEST_DIR)/inflate-test \
		$(UNI_CODEC_TEST_DIR)/xz-test \
		$(UNI_CODEC_TEST_DIR)/payload.gz \
		$(UNI_CODEC_TEST_DIR)/payload-crc64.xz \
		$(UNI_CODEC_TEST_DIR)/payload-crc32.xz \
		$(UNI_CODEC_TEST_DIR)/payload-none.xz \
		$(UNI_CODEC_TEST_DIR)/payload-x86.xz
	$(UNI_CODEC_TEST_DIR)/inflate-test $(UNI_CODEC_TEST_DIR)/payload.gz README.md
	$(UNI_CODEC_TEST_DIR)/xz-test $(UNI_CODEC_TEST_DIR)/payload-crc64.xz README.md
	$(UNI_CODEC_TEST_DIR)/xz-test $(UNI_CODEC_TEST_DIR)/payload-crc32.xz README.md
	$(UNI_CODEC_TEST_DIR)/xz-test $(UNI_CODEC_TEST_DIR)/payload-none.xz README.md
	$(UNI_CODEC_TEST_DIR)/xz-test $(UNI_CODEC_TEST_DIR)/payload-x86.xz README.md
	head -c -1 $(UNI_CODEC_TEST_DIR)/payload.gz > $(UNI_CODEC_TEST_DIR)/truncated.gz
	if $(UNI_CODEC_TEST_DIR)/inflate-test $(UNI_CODEC_TEST_DIR)/truncated.gz README.md; then exit 1; fi
	head -c -1 $(UNI_CODEC_TEST_DIR)/payload-crc64.xz > $(UNI_CODEC_TEST_DIR)/truncated.xz
	if $(UNI_CODEC_TEST_DIR)/xz-test $(UNI_CODEC_TEST_DIR)/truncated.xz README.md; then exit 1; fi
	@printf 'Uni gzip/XZ codec tests passed\n'

.PHONY: all clean test-uni-codecs run run-headless run-64m run-32m run-16m run-curses
