/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <rtc.h>
#include <stddef.h>

#define CMOS_INDEX 0x70
#define CMOS_DATA 0x71
#define RTC_SECOND 0x00
#define RTC_MINUTE 0x02
#define RTC_HOUR 0x04
#define RTC_WEEKDAY 0x06
#define RTC_DAY 0x07
#define RTC_MONTH 0x08
#define RTC_YEAR 0x09
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B
#define RTC_CENTURY 0x32

extern void outb(uint16_t port, uint8_t val);

static inline uint8_t inb_local(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_INDEX, (uint8_t)(0x80 | reg));
    return inb_local(CMOS_DATA);
}

static int rtc_update_in_progress(void) {
    return (cmos_read(RTC_STATUS_A) & 0x80) != 0;
}

static uint8_t bcd_to_binary(uint8_t value) {
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

static int same_sample(const struct rtc_time* a, const struct rtc_time* b) {
    return a->second == b->second &&
           a->minute == b->minute &&
           a->hour == b->hour &&
           a->day == b->day &&
           a->month == b->month &&
           a->year == b->year &&
           a->weekday == b->weekday;
}

static void read_sample(struct rtc_time* out) {
    out->second = cmos_read(RTC_SECOND);
    out->minute = cmos_read(RTC_MINUTE);
    out->hour = cmos_read(RTC_HOUR);
    out->weekday = cmos_read(RTC_WEEKDAY);
    out->day = cmos_read(RTC_DAY);
    out->month = cmos_read(RTC_MONTH);
    out->year = cmos_read(RTC_YEAR);
}

int rtc_read_time(struct rtc_time* out) {
    struct rtc_time first;
    struct rtc_time second;
    uint8_t status_b;
    uint8_t century;

    if (!out) {
        return -1;
    }

    for (int tries = 0; tries < 100000 && rtc_update_in_progress(); tries++) {
        asm volatile("pause");
    }

    do {
        read_sample(&first);
        for (int tries = 0; tries < 100000 && rtc_update_in_progress(); tries++) {
            asm volatile("pause");
        }
        read_sample(&second);
    } while (!same_sample(&first, &second));

    status_b = cmos_read(RTC_STATUS_B);
    century = cmos_read(RTC_CENTURY);

    if ((status_b & 0x04) == 0) {
        second.second = bcd_to_binary(second.second);
        second.minute = bcd_to_binary(second.minute);
        second.hour = (uint8_t)((second.hour & 0x80) | bcd_to_binary((uint8_t)(second.hour & 0x7F)));
        second.weekday = bcd_to_binary(second.weekday);
        second.day = bcd_to_binary(second.day);
        second.month = bcd_to_binary(second.month);
        second.year = bcd_to_binary((uint8_t)second.year);
        century = bcd_to_binary(century);
    }

    if ((status_b & 0x02) == 0 && (second.hour & 0x80)) {
        second.hour = (uint8_t)(((second.hour & 0x7F) + 12) % 24);
    }

    if (century >= 19 && century <= 99) {
        second.year = (uint16_t)(century * 100 + second.year);
    } else {
        second.year = (uint16_t)(2000 + second.year);
        if (second.year < 2020) {
            second.year = (uint16_t)(second.year + 100);
        }
    }

    *out = second;
    return 0;
}
