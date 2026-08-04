/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#ifndef THERMAL_H
#define THERMAL_H

#include <stdint.h>

struct thermal_cpu_sample {
    uint8_t present;
    uint8_t temp_known;
    int32_t temp_celsius;
};

struct thermal_status {
    uint8_t sensor_present;
    uint8_t temp_known;
    uint8_t emergency_active;
    uint8_t hardware_throttling;
    uint8_t critical_active;
    uint8_t package_sensor_present;
    uint16_t reserved;
    int32_t current_celsius;
    int32_t maximum_celsius;
    int32_t tjmax_celsius;
    int32_t emergency_celsius;
    int32_t clear_celsius;
    uint64_t sample_ticks;
};

void thermal_init(void);
int thermal_cpu_read(uint32_t core_id, struct thermal_cpu_sample* out);
int thermal_timer_tick(void);
int thermal_get_status(struct thermal_status* out);
int thermal_emergency_active(void);

#endif
