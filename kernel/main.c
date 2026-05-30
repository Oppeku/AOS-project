/* SPDX-License-Identifier: GPL-3.0
 * Copyright (C) 2026 Abhigyan Narayan
 *AOS Kernel Version-1.0
*/
//start of the file 
#include <vga.h>
#include <gdt.h>
#include <idt.h>
#include <pmm.h>
#include <vmm.h>
#include <cpio.h>
#include <aosfs.h>
#include <ata.h>
#include <ext4.h>
#include <fat32.h>
#include <tmpfs.h>
#include <vfs.h>
#include <elf64_loader.h>
#include <multiboot2.h>
#include <panic.h>
#include <partition.h>
#include <blkdev.h>
#include <pci.h>
#include <driver.h>
#include <firmware.h>
#include <input.h>
#include <netdev.h>
#include <e1000.h>
#include <wifi.h>
#include <xhci.h>
#include <process.h>
#include <timer.h>
#include <tty.h>
#include <stdint.h>
#include <stddef.h>

/*
 * Embedded ownership marker.
 * This does not affect runtime behavior, but leaves a recoverable signature
 * in the kernel binary for later inspection with tools like strings/readelf.
 */
__attribute__((used))
static const char aos_kernel_watermark[] =
    "AOS-WATERMARK|owner=Abhigyan Narayan|project=AOS|component=kernel|issued=2026-04-28|uuid=";

#ifndef AOS_BOOT_VERBOSE
#define AOS_BOOT_VERBOSE 0
#endif

#ifndef AOS_ENABLE_MUI_FRAMEBUFFER
#define AOS_ENABLE_MUI_FRAMEBUFFER 0
#endif

uint8_t aos_boot_verbose = AOS_BOOT_VERBOSE;

void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

void serial_print(const char* s) {
    for (int i = 0; s[i] != '\0'; i++) {
        outb(0x3F8, s[i]);
    }
}

static size_t local_strlen(const char* s) {
    size_t len = 0;
    while (s && s[len] != '\0') {
        len++;
    }
    return len;
}

static void build_padding(char* out, size_t out_size, size_t count, char fill) {
    size_t i;

    if (!out || out_size == 0) {
        return;
    }
    if (count + 1 > out_size) {
        count = out_size - 1;
    }

    for (i = 0; i < count; i++) {
        out[i] = fill;
    }
    out[count] = '\0';
}

static void boot_log_status(const char* message, const char* tag, unsigned char tag_color) {
    char padding[48];
    size_t message_len = local_strlen(message);
    size_t target_width = 52;
    size_t pad_count = (message_len < target_width) ? (target_width - message_len) : 2;

    build_padding(padding, sizeof(padding), pad_count, '.');

    serial_print(message);
    serial_print(" ");
    serial_print(tag);
    serial_print("\n");

    vga_write(message, 0x07);
    vga_write(" ", 0x08);
    vga_write(padding, 0x08);
    vga_write(" ", 0x08);
    vga_write(tag, tag_color);
    vga_write("\n", 0x07);
}

static void boot_log_started(const char* message) {
    if (!aos_boot_verbose) return;
    boot_log_status(message, "[ START ]", 0x0B);
}

static void boot_log_ok(const char* message) {
    if (!aos_boot_verbose) return;
    boot_log_status(message, "[  OK   ]", 0x0A);
}

static void boot_log_warn(const char* message) {
    if (!aos_boot_verbose) return;
    boot_log_status(message, "[ WARN  ]", 0x0E);
}

static void boot_log_fail(const char* message) {
    boot_log_status(message, "[ FAIL  ]", 0x0C);
}

static void serial_print_hex64(uint64_t value) {
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        uint8_t nibble = (value >> ((15 - i) * 4)) & 0xF;
        buf[2 + i] = (nibble < 10) ? ('0' + nibble) : ('A' + (nibble - 10));
    }
    buf[18] = '\0';
    serial_print(buf);
}

static void serial_print_u64(uint64_t value) {
    char buf[21];
    int pos = 20;

    buf[pos] = '\0';
    if (value == 0) {
        serial_print("0");
        return;
    }

    while (value > 0 && pos > 0) {
        pos--;
        buf[pos] = (char)('0' + (value % 10));
        value /= 10;
    }

    serial_print(&buf[pos]);
}

static void serial_print_mib(uint64_t bytes) {
    serial_print_u64((bytes + 1048575ULL) / 1048576ULL);
    serial_print(" MiB");
}

static void boot_log_memory_snapshot(const char* label) {
    if (!aos_boot_verbose) return;

    serial_print(label);
    serial_print(": total=");
    serial_print_mib(pmm_total_memory());
    serial_print(" used=");
    serial_print_mib(pmm_used_memory());
    serial_print(" free=");
    serial_print_mib(pmm_free_memory());
    serial_print("\n");
}

static void ensure_default_user_layout(void) {
    const char* dirs[] = {
        "root",
    };

    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        (void)aosfs_mkdir_path(dirs[i]);
    }
}

static void seed_file_if_missing(const char* path, const char* data) {
    struct vfs_node node;
    uint64_t written = 0;
    uint32_t new_size = 0;

    if (!path || !data || vfs_lookup(path, &node) == 0) {
        return;
    }
    if (aosfs_create_path(path, &node) != 0) {
        return;
    }
    (void)aosfs_write_path(path, 0, (const uint8_t*)data, local_strlen(data), &written, &new_size);
}

static void seed_default_user_database(void) {
    seed_file_if_missing("etc/passwd", "root:x:0:0:AOS Live Root:/root:/commands/shell.elf\n");
    seed_file_if_missing("etc/group", "root:x:0:root\n");
    seed_file_if_missing("etc/shadow", "root::0:0:99999:7:::\n");
    seed_file_if_missing("etc/sudoers", "root ALL=(ALL) NOPASSWD: ALL\n");
}

extern void jump_to_user(uint64_t code, uint64_t stack);
extern void syscall_entry();

struct cpu_context {
    uint64_t kernel_stack;
    uint64_t user_stack;
} __attribute__((packed));

struct cpu_context cpu0;

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = dest;
    const uint8_t* s = src;
    while (n--) *d++ = *s++;
    return dest;
}

static void init_pic() {
    // ICW1
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    // ICW2 (Remap)
    outb(0x21, 0x20); // Master PIC -> 0x20
    outb(0xA1, 0x28); // Slave PIC -> 0x28
    // ICW3
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    // ICW4
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    
    // Mask all but Timer (IRQ0) and Keyboard (IRQ1)
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

void kernel_main(uint64_t magic, uint64_t mb_info) {
    (void)magic;
    vga_clear(0x07);
    if (aos_boot_verbose) {
        serial_print("AOS: Kernel Main Started\n");
    }
    boot_log_started("Starting AOS kernel boot");
    pmm_init(mb_info);
    boot_log_ok("Starting physical memory manager");
    init_vmm();
    boot_log_ok("Starting virtual memory manager");
    init_gdt();
    boot_log_ok("Starting global descriptor table");
    init_idt();
    boot_log_ok("Starting interrupt descriptor table");
    init_process();
    boot_log_ok("Starting process management");
    pci_init();
    boot_log_ok("Starting PCI discovery");
    driver_init();
    netdev_init();
    driver_register_system(DRIVER_CLASS_CORE, "aos-pmm", "ready: physical memory manager");
    driver_register_system(DRIVER_CLASS_CORE, "aos-vmm", "ready: virtual memory manager");
    driver_register_system(DRIVER_CLASS_CORE, "aos-gdt", "ready: descriptor tables");
    driver_register_system(DRIVER_CLASS_CORE, "aos-idt", "ready: interrupt descriptors");
    driver_register_system(DRIVER_CLASS_CORE, "aos-process", "ready: process manager");
    driver_register_system(DRIVER_CLASS_CORE, "aos-pci", "ready: pci discovery");
    driver_import_pci_devices();
    e1000_register_driver();
    wifi_register_driver();
    xhci_register_driver();
    driver_register_system(DRIVER_CLASS_NETWORK, "aos-netdev", "ready: network device registry");
    boot_log_ok("Starting driver model");
    blkdev_init();
    driver_register_system(DRIVER_CLASS_STORAGE, "aos-blkdev", "ready: block device layer");
    boot_log_ok("Starting block device layer");
    uint32_t ata0_id = ata_init_primary_master();
    if (ata0_id != BLKDEV_INVALID_ID) {
        driver_register_system(DRIVER_CLASS_STORAGE, "aos-ata-pio", "ready: primary master ata0");
        boot_log_ok("Starting ATA primary master");
    } else {
        driver_register_system(DRIVER_CLASS_STORAGE, "aos-ata-pio", "not found: primary master");
        boot_log_warn("Starting ATA primary master");
    }
    partition_init();
    driver_register_system(DRIVER_CLASS_STORAGE, "aos-partition", "ready: AOS partition table");
    if (ata0_id != BLKDEV_INVALID_ID) {
        const struct blkdev* ata0 = blkdev_get(ata0_id);
        if (partition_load_table(ata0_id) > 0) {
            boot_log_ok("Loading AOS partition table from ata0");
        } else if (ata0 && ata0->size > 512) {
            (void)partition_register_blkdev(ata0_id, 512, ata0->size - 512, PARTITION_FS_AOSFS, "ata0p0");
            boot_log_warn("Loading AOS partition table from ata0");
        }
    }
    boot_log_ok("Starting PartiotionMANAGAER");
    vfs_init_mounts();
    driver_register_system(DRIVER_CLASS_FILESYSTEM, "aos-vfs", "ready: mount table and path walking");
    boot_log_ok("Starting VFS mount table");
    init_timer(100);
    driver_register_system(DRIVER_CLASS_TIME, "aos-timer", "ready: 100 Hz system timer");
    driver_register_system(DRIVER_CLASS_TIME, "aos-rtc", "ready: cmos clock");
    boot_log_ok("Starting system timer at 100 Hz");
    init_pic();
    driver_register_system(DRIVER_CLASS_CORE, "aos-pic", "ready: irq remap");
    boot_log_ok("Starting PIC remap");
    asm volatile("sti");
    driver_register_system(DRIVER_CLASS_CORE, "aos-interrupts", "ready: hardware interrupts");
    driver_register_system(DRIVER_CLASS_INPUT, "aos-keyboard", "ready: ps/2 keyboard input");
    boot_log_ok("Starting hardware interrupts");

    // Handle boot modules: initrd first, remaining modules as RAM-backed partitions.
    struct multiboot_tag* tag;
    uint32_t module_index = 0;
    uint8_t initrd_ready = 0;
    uint8_t fat32_ready = 0;
    uint8_t fat32_seen = 0;
    uint8_t ext4_ready = 0;
    uint8_t ext4_seen = 0;
    boot_log_started("Starting boot module scan");
    for (tag = (struct multiboot_tag*)(mb_info + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) 
    {
        if (tag->size < sizeof(struct multiboot_tag)) {
            boot_log_warn("Stopping boot module scan");
            break;
        }
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module* mod = (struct multiboot_tag_module*)tag;
            if (module_index == 0) {
                if (init_initrd(mod->mod_start, mod->mod_end) != 0) {
                    boot_log_fail("Starting initrd mount");
                    aos_panic("Boot failure", "Could not mount the initrd boot module.");
                }
                initrd_ready = 1;
                driver_register_system(DRIVER_CLASS_FILESYSTEM, "aos-initrd", "ready: cpio boot archive");
                firmware_init();
                if (firmware_count() > 0) {
                    driver_register_system(DRIVER_CLASS_NETWORK, "aos-firmware", "ready: firmware blobs discovered");
                } else {
                    driver_register_system(DRIVER_CLASS_NETWORK, "aos-firmware", "ready: no firmware blobs bundled");
                }
                boot_log_ok("Starting initrd mount");
            } else {
                char part_name[16] = "ram0p0";
                part_name[5] = (char)('0' + (module_index - 1));
                if (partition_register_memory(mod->mod_start, mod->mod_end, part_name) < 0) {
                    boot_log_warn("Registering boot partition");
                }
            }
            module_index++;
        }
    }
    if (!initrd_ready) {
        boot_log_fail("Starting initrd mount");
        aos_panic("Boot failure", "The initrd boot module is missing.");
    }
    if (AOS_ENABLE_MUI_FRAMEBUFFER) {
        vga_init_framebuffer(mb_info);
        driver_register_system(DRIVER_CLASS_DISPLAY, "aos-framebuffer", "ready: MUI framebuffer console");
        if (aos_boot_verbose) {
            boot_log_ok("Starting framebuffer console");
        }
    } else {
        vga_init_tty();
        driver_register_system(DRIVER_CLASS_DISPLAY, "aos-vga-tty", "ready: VGA text console");
        if (aos_boot_verbose) {
            boot_log_ok("Starting TTY console");
        }
    }
    input_init();
    driver_register_system(DRIVER_CLASS_INPUT, "aos-input", "ready: keyboard event queue");
    tty_init();
    driver_register_system(DRIVER_CLASS_CORE, "aos-tty", "ready: terminal line discipline");
    if (aos_boot_verbose) {
        boot_log_ok("Starting TTY layer");
    }
    if (aos_boot_verbose) {
        partition_print_table();
    }

    const struct blkdev* aosfs_disk = blkdev_find("ata0");
    const struct partition* aosfs_part = partition_find_by_role(PARTITION_ROLE_ROOT);
    if (!aosfs_part) {
        aosfs_part = partition_find_by_fs(PARTITION_FS_AOSFS);
    }
    if (aosfs_disk && aosfs_part && aosfs_part->blkdev_id == aosfs_disk->id &&
        aosfs_mount_at(aosfs_part->blkdev_id, aosfs_part->offset) == 0) {
        driver_register_system(DRIVER_CLASS_FILESYSTEM, "aos-aosfs", "ready: root filesystem mounted from ata0");
        boot_log_ok("Starting AOSFS mount at / from ata0");
    } else if (aosfs_part) {
        if (aosfs_mount(aosfs_part->blkdev_id) == 0) {
            driver_register_system(DRIVER_CLASS_FILESYSTEM, "aos-aosfs", "ready: root filesystem mounted");
            boot_log_ok("Starting AOSFS mount at /");
        } else {
            driver_register_system(DRIVER_CLASS_FILESYSTEM, "aos-aosfs", "warn: root mount failed");
            boot_log_warn("Starting AOSFS mount at /");
        }
    } else {
        driver_register_system(DRIVER_CLASS_FILESYSTEM, "aos-aosfs", "warn: root partition missing");
        boot_log_warn("Starting AOSFS mount at /");
    }

    const struct partition* main_part = partition_find_by_role(PARTITION_ROLE_MAIN);
    if (main_part && main_part->fs_type == PARTITION_FS_AOSFS &&
        aosfs_mount_role(PARTITION_ROLE_MAIN, main_part->blkdev_id, main_part->offset) == 0) {
        (void)vfs_mount("main", VFS_BACKEND_AOSFS, "@main");
        boot_log_ok("Starting AOSFS mount at /main");
    }

    const struct partition* etc_part = partition_find_by_role(PARTITION_ROLE_ETC);
    if (etc_part && etc_part->fs_type == PARTITION_FS_AOSFS &&
        aosfs_mount_role(PARTITION_ROLE_ETC, etc_part->blkdev_id, etc_part->offset) == 0) {
        (void)vfs_mount("etc", VFS_BACKEND_AOSFS, "@etc");
        boot_log_ok("Starting AOSFS mount at /etc");
    }

    const struct partition* fat32_part = partition_find_by_fs(PARTITION_FS_FAT32);
    if (fat32_part) {
        fat32_seen = 1;
        if (fat32_init(fat32_part->start, fat32_part->end) == 0) {
            fat32_ready = 1;
            driver_register_system(DRIVER_CLASS_FILESYSTEM, "aos-fat32", "ready: mounted at /mnt/fat32");
            boot_log_ok("Starting FAT32 mount at /mnt/fat32");
        } else {
            driver_register_system(DRIVER_CLASS_FILESYSTEM, "aos-fat32", "warn: mount failed");
            boot_log_warn("Starting FAT32 mount at /mnt/fat32");
        }
    }

    const struct partition* ext4_part = partition_find_by_fs(PARTITION_FS_EXT4);
    if (ext4_part) {
        ext4_seen = 1;
        if (ext4_init(ext4_part->start, ext4_part->end) == 0) {
            ext4_ready = 1;
            driver_register_system(DRIVER_CLASS_FILESYSTEM, "aos-ext4", "ready: mounted at /mnt/ext4");
            boot_log_ok("Starting EXT4 mount at /mnt/ext4");
        } else {
            driver_register_system(DRIVER_CLASS_FILESYSTEM, "aos-ext4", "warn: mount failed");
            boot_log_warn("Starting EXT4 mount at /mnt/ext4");
        }
    }

    if (!fat32_ready && !fat32_seen) {
        boot_log_warn("Starting FAT32 mount at /mnt/fat32");
    }
    if (!ext4_ready && !ext4_seen) {
        boot_log_warn("Starting EXT4 mount at /mnt/ext4");
    }

    const struct partition* main_storage_part = partition_find_by_role(PARTITION_ROLE_MAIN);
    if (main_storage_part && main_storage_part->fs_type == PARTITION_FS_EXT4) {
        boot_log_ok("AOS layout: /main is ext4 user storage");
    }
    const struct partition* trash_part = partition_find_by_role(PARTITION_ROLE_TRASH);
    if (trash_part && trash_part->fs_type == PARTITION_FS_FAT32) {
        boot_log_ok("AOS layout: /trash is FAT32 recovery storage");
    }

    ensure_default_user_layout();
    boot_log_ok("Starting default user home layout");
    seed_default_user_database();
    boot_log_ok("Starting default user database");

    tmpfs_init();
    driver_register_system(DRIVER_CLASS_FILESYSTEM, "aos-tmpfs", "ready: mounted at /tmp");
    boot_log_ok("Starting tmpfs mount at /tmp");

    // Setup Kernel Stack for GS
    extern uint64_t stack_top;
    cpu0.kernel_stack = (uint64_t)&stack_top;
    uint64_t gs_base = (uint64_t)&cpu0;
    asm volatile ("wrmsr" : : "c"(0xC0000102), "a"((uint32_t)gs_base), "d"((uint32_t)(gs_base >> 32)));

    extern void init_syscall();
    init_syscall();
    driver_register_system(DRIVER_CLASS_CORE, "aos-syscall", "ready: userspace syscall entry");
    boot_log_ok("Starting syscall entry");

    // Load user ELF from initrd and map PT_LOAD segments.
    extern uint64_t p4_table[];
    boot_log_started("Starting shell.elf lookup");
    uint8_t* user_elf = 0;
    uint32_t user_elf_size = 0;
    if (initrd_get_file("shell.elf", &user_elf, &user_elf_size) != 0) {
        boot_log_fail("Starting shell.elf lookup");
        aos_panic("Boot failure", "shell.elf is missing from the initrd.");
    }
    boot_log_ok("Starting shell.elf lookup");

    uint64_t user_entry = 0;
    boot_log_started("Starting shell.elf load");
    if (elf64_load_image(p4_table, user_elf, user_elf_size, &user_entry) != 0) {
        boot_log_fail("Starting shell.elf load");
        aos_panic("Boot failure", "ELF loader rejected shell.elf.");
    }
    boot_log_ok("Starting shell.elf load");
    if (aos_boot_verbose) {
        serial_print("AOS: user entry @ ");
        serial_print_hex64(user_entry);
        serial_print("\n");
    }

    uint64_t ustack_base = 0x70000080000ULL;
    // Map 2 pages for stack
    void* ustack_page0 = pmm_alloc_block();
    void* ustack_page1 = pmm_alloc_block();
    if (!ustack_page0 || !ustack_page1) {
        boot_log_fail("Starting userspace stack mapping");
        aos_panic("Boot failure", "Failed to allocate mapped userspace stack pages.");
    }
    vmm_map_page(p4_table, ustack_base, (uint64_t)ustack_page0, 0x7);
    vmm_map_page(p4_table, ustack_base + 0x1000, (uint64_t)ustack_page1, 0x7);
    uint64_t ustack_top = ustack_base + 0x2000;
    boot_log_ok("Starting userspace stack mapping");
    boot_log_memory_snapshot("AOS memory before shell");
    boot_log_ok("Starting shell.elf");

    jump_to_user(user_entry, ustack_top);

    while(1);
}
//end of the file 
