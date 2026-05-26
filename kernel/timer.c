#include <stdint.h>

extern void outb(uint16_t port, uint8_t val);

static volatile uint64_t g_timer_ticks;
static uint32_t g_timer_frequency = 100;

void init_timer(uint32_t frequency) {
    if (frequency == 0) {
        frequency = 100;
    }
    g_timer_frequency = frequency;
    g_timer_ticks = 0;

    uint32_t divisor = 1193180 / frequency;

    // Send the command byte
    outb(0x43, 0x36);

    // Divisor must be sent byte by byte
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, l);
    outb(0x40, h);
}

void timer_tick(void) {
    g_timer_ticks++;
}

uint64_t timer_get_ticks(void) {
    return g_timer_ticks;
}

uint32_t timer_get_frequency(void) {
    return g_timer_frequency;
}
