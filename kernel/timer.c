#include <stdint.h>

extern void outb(uint16_t port, uint8_t val);

void init_timer(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;

    // Send the command byte
    outb(0x43, 0x36);

    // Divisor must be sent byte by byte
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, l);
    outb(0x40, h);
}
