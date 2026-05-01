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
#include <ext4.h>
#include <fat32.h>
#include <tmpfs.h>
#include <vfs.h>
#include <elf64_loader.h>
#include <multiboot2.h>
#include <panic.h>
#include <process.h>
#include <stdint.h>
#include <stddef.h>

/*
 * Embedded ownership marker.
 * This does not affect runtime behavior, but leaves a recoverable signature
 * in the kernel binary for later inspection with tools like strings/readelf.
 */
__attribute__((used))
static const char aos_kernel_watermark[] =
    "AOS-WATERMARK|owner=Abhigyan Narayan|project=AOS|component=kernel|issued=2026-04-28|uuid=5d9d6d7b-77b3-4d66-b38d-7b0d7d3d6a51";

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
    boot_log_status(message, "[ START ]", 0x0B);
}

static void boot_log_ok(const char* message) {
    boot_log_status(message, "[  OK   ]", 0x0A);
}

static void boot_log_warn(const char* message) {
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
    serial_print("AOS: Kernel Main Started\n");
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
    vfs_init_mounts();
    boot_log_ok("Starting VFS mount table");
    extern void init_timer(uint32_t);
    init_timer(100);
    boot_log_ok("Starting system timer at 100 Hz");
    init_pic();
    boot_log_ok("Starting PIC remap");
    asm volatile("sti");
    boot_log_ok("Starting hardware interrupts");

    // Handle boot modules: initrd first, FAT32 second, EXT4 third.
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
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
            struct multiboot_tag_module* mod = (struct multiboot_tag_module*)tag;
            if (module_index == 0) {
                if (init_initrd(mod->mod_start, mod->mod_end) != 0) {
                    boot_log_fail("Starting initrd mount");
                    aos_panic("Boot failure", "Could not mount the initrd boot module.");
                }
                initrd_ready = 1;
                boot_log_ok("Starting initrd mount");
            } else if (module_index == 1) {
                fat32_seen = 1;
                if (fat32_init(mod->mod_start, mod->mod_end) == 0) {
                    fat32_ready = 1;
                    boot_log_ok("Starting FAT32 mount at /mnt/fat32");
                } else {
                    boot_log_warn("Starting FAT32 mount at /mnt/fat32");
                }
            } else if (module_index == 2) {
                ext4_seen = 1;
                if (ext4_init(mod->mod_start, mod->mod_end) == 0) {
                    ext4_ready = 1;
                    boot_log_ok("Starting EXT4 mount at /mnt/ext4");
                } else {
                    boot_log_warn("Starting EXT4 mount at /mnt/ext4");
                }
            }
            module_index++;
        }
    }
    if (!initrd_ready) {
        boot_log_fail("Starting initrd mount");
        aos_panic("Boot failure", "The initrd boot module is missing.");
    }
    if (!fat32_ready && !fat32_seen) {
        boot_log_warn("Starting FAT32 mount at /mnt/fat32");
    }
    if (!ext4_ready && !ext4_seen) {
        boot_log_warn("Starting EXT4 mount at /mnt/ext4");
    }

    tmpfs_init();
    boot_log_ok("Starting tmpfs mount at /tmp");

    // Setup Kernel Stack for GS
    extern uint64_t stack_top;
    cpu0.kernel_stack = (uint64_t)&stack_top;
    uint64_t gs_base = (uint64_t)&cpu0;
    asm volatile ("wrmsr" : : "c"(0xC0000102), "a"((uint32_t)gs_base), "d"((uint32_t)(gs_base >> 32)));

    extern void init_syscall();
    init_syscall();
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
    serial_print("AOS: user entry @ ");
    serial_print_hex64(user_entry);
    serial_print("\n");

    void* ustack_phys = pmm_alloc_block();
    if (!ustack_phys) {
        boot_log_fail("Starting userspace stack allocation");
        aos_panic("Boot failure", "Failed to allocate the first userspace stack page.");
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
    boot_log_ok("Starting shell.elf");

    jump_to_user(user_entry, ustack_top);

    while(1);
}
//end of the file 
