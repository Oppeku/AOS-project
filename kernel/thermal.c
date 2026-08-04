/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Oppeko
 */

#include <thermal.h>
#include <driver.h>
#include <timer.h>

#define MSR_IA32_THERM_STATUS 0x19C
#define MSR_IA32_TEMPERATURE_TARGET 0x1A2
#define MSR_IA32_PACKAGE_THERM_STATUS 0x1B1
#define THERMAL_REQUESTED_EMERGENCY_C 120
#define THERMAL_TJMAX_HEADROOM_C 5
#define THERMAL_CLEAR_HYSTERESIS_C 10

static uint8_t g_thermal_ready;
static uint8_t g_has_digital_thermal_sensor;
static uint8_t g_has_package_thermal_sensor;
static uint8_t g_temp_known;
static uint8_t g_emergency_active;
static uint8_t g_hardware_throttling;
static uint8_t g_critical_active;
static int32_t g_tjmax_celsius = 100;
static int32_t g_current_celsius;
static int32_t g_maximum_celsius;
static int32_t g_emergency_celsius = 95;
static int32_t g_clear_celsius = 85;
static uint64_t g_sample_ticks;
static uint32_t g_poll_ticks;

extern void serial_print(const char* s);

static void cpuid_count(uint32_t leaf, uint32_t subleaf,
                        uint32_t* eax, uint32_t* ebx,
                        uint32_t* ecx, uint32_t* edx) {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;

    __asm__ volatile("cpuid"
                     : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(leaf), "c"(subleaf));

    if (eax) *eax = a;
    if (ebx) *ebx = b;
    if (ecx) *ecx = c;
    if (edx) *edx = d;
}

static uint64_t rdmsr_u64(uint32_t msr) {
    uint32_t low;
    uint32_t high;

    __asm__ volatile("rdmsr"
                     : "=a"(low), "=d"(high)
                     : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static int thermal_read_msr(uint32_t msr, int32_t* temperature,
                            uint8_t* throttling, uint8_t* critical) {
    uint64_t status = rdmsr_u64(msr);
    int32_t delta;
    int32_t value;

    if ((status & (1ULL << 31)) == 0) return -1;
    delta = (int32_t)((status >> 16) & 0x7F);
    value = g_tjmax_celsius - delta;
    if (value < -50 || value > 150) return -1;

    if (temperature) *temperature = value;
    if (throttling) *throttling = (status & 1ULL) ? 1U : 0U;
    if (critical) *critical = (status & (1ULL << 4)) ? 1U : 0U;
    return 0;
}

static void thermal_sample_now(void) {
    int32_t core_temp = 0;
    int32_t package_temp = 0;
    uint8_t core_throttling = 0;
    uint8_t package_throttling = 0;
    uint8_t core_critical = 0;
    uint8_t package_critical = 0;
    uint8_t core_known = 0;
    uint8_t package_known = 0;
    uint8_t was_emergency = g_emergency_active;

    g_temp_known = 0;
    g_hardware_throttling = 0;
    g_critical_active = 0;
    g_sample_ticks = timer_get_ticks();

    if (!g_has_digital_thermal_sensor) return;
    if (thermal_read_msr(MSR_IA32_THERM_STATUS, &core_temp,
                         &core_throttling, &core_critical) == 0) {
        core_known = 1;
        g_current_celsius = core_temp;
        g_maximum_celsius = core_temp;
    }
    if (g_has_package_thermal_sensor &&
        thermal_read_msr(MSR_IA32_PACKAGE_THERM_STATUS, &package_temp,
                         &package_throttling, &package_critical) == 0) {
        package_known = 1;
        if (!core_known || package_temp > g_maximum_celsius) {
            g_maximum_celsius = package_temp;
        }
        if (!core_known) g_current_celsius = package_temp;
    }

    g_temp_known = core_known || package_known;
    g_hardware_throttling = core_throttling || package_throttling;
    g_critical_active = core_critical || package_critical;

    if (!g_emergency_active) {
        if (g_hardware_throttling || g_critical_active ||
            (g_temp_known && g_maximum_celsius >= g_emergency_celsius)) {
            g_emergency_active = 1;
        }
    } else if (!g_hardware_throttling && !g_critical_active &&
               g_temp_known && g_maximum_celsius <= g_clear_celsius) {
        g_emergency_active = 0;
    }

    if (!was_emergency && g_emergency_active) {
        serial_print("thermal: emergency process limiting enabled\n");
    } else if (was_emergency && !g_emergency_active) {
        serial_print("thermal: temperature recovered, process limiting disabled\n");
    }
}

void thermal_init(void) {
    uint32_t max_leaf = 0;
    uint32_t eax = 0;
    uint32_t ebx = 0;
    uint32_t ecx = 0;
    uint32_t edx = 0;

    uint32_t vendor_ebx;
    uint32_t vendor_ecx;
    uint32_t vendor_edx;
    uint8_t genuine_intel;
    uint8_t has_msr = 0;
    uint8_t hypervisor = 0;

    cpuid_count(0, 0, &max_leaf, &vendor_ebx, &vendor_ecx, &vendor_edx);
    genuine_intel = vendor_ebx == 0x756E6547U &&
                    vendor_edx == 0x49656E69U &&
                    vendor_ecx == 0x6C65746EU;
    g_thermal_ready = 1;
    g_has_digital_thermal_sensor = 0;
    g_has_package_thermal_sensor = 0;
    g_temp_known = 0;
    g_emergency_active = 0;
    g_hardware_throttling = 0;
    g_critical_active = 0;
    g_tjmax_celsius = 100;
    g_current_celsius = 0;
    g_maximum_celsius = 0;
    g_emergency_celsius = THERMAL_REQUESTED_EMERGENCY_C;
    g_clear_celsius = THERMAL_REQUESTED_EMERGENCY_C -
                       THERMAL_CLEAR_HYSTERESIS_C;
    g_poll_ticks = 0;

    if (max_leaf >= 1) {
        cpuid_count(1, 0, &eax, &ebx, &ecx, &edx);
        has_msr = (edx & (1U << 5)) ? 1U : 0U;
        hypervisor = (ecx & (1U << 31)) ? 1U : 0U;
    }

    if (genuine_intel && has_msr && !hypervisor && max_leaf >= 6) {
        cpuid_count(6, 0, &eax, &ebx, &ecx, &edx);
        g_has_digital_thermal_sensor = (eax & 1U) ? 1U : 0U;
        g_has_package_thermal_sensor = (eax & (1U << 6)) ? 1U : 0U;
    }

    if (g_has_digital_thermal_sensor) {
        uint64_t target = rdmsr_u64(MSR_IA32_TEMPERATURE_TARGET);
        int32_t tjmax = (int32_t)((target >> 16) & 0xFF);
        if (tjmax >= 60 && tjmax <= 125) {
            g_tjmax_celsius = tjmax;
        }
        if (g_tjmax_celsius - THERMAL_TJMAX_HEADROOM_C <
            g_emergency_celsius) {
            g_emergency_celsius =
                g_tjmax_celsius - THERMAL_TJMAX_HEADROOM_C;
        }
        g_clear_celsius =
            g_emergency_celsius - THERMAL_CLEAR_HYSTERESIS_C;
        thermal_sample_now();
        driver_register_system(DRIVER_CLASS_TIME, "aos-thermal", "ready: cpu digital thermal sensor");
    } else {
        driver_register_system(DRIVER_CLASS_TIME, "aos-thermal", "ready: thermal sensor unavailable");
    }
}

int thermal_cpu_read(uint32_t core_id, struct thermal_cpu_sample* out) {
    if (!out) return -1;
    out->present = g_thermal_ready && g_has_digital_thermal_sensor;
    out->temp_known = 0;
    out->temp_celsius = 0;

    if (core_id != 0 || !g_has_digital_thermal_sensor) {
        return 0;
    }
    out->temp_celsius = g_current_celsius;
    out->temp_known = g_temp_known;
    return 0;
}

int thermal_timer_tick(void) {
    uint32_t frequency = timer_get_frequency();

    if (!g_thermal_ready) return 0;
    if (frequency == 0) frequency = 100;
    g_poll_ticks++;
    if (g_poll_ticks < frequency) return 0;
    g_poll_ticks = 0;
    thermal_sample_now();
    return 1;
}

int thermal_get_status(struct thermal_status* out) {
    if (!out) return -1;
    out->sensor_present = g_has_digital_thermal_sensor;
    out->temp_known = g_temp_known;
    out->emergency_active = g_emergency_active;
    out->hardware_throttling = g_hardware_throttling;
    out->critical_active = g_critical_active;
    out->package_sensor_present = g_has_package_thermal_sensor;
    out->reserved = 0;
    out->current_celsius = g_current_celsius;
    out->maximum_celsius = g_maximum_celsius;
    out->tjmax_celsius = g_tjmax_celsius;
    out->emergency_celsius = g_emergency_celsius;
    out->clear_celsius = g_clear_celsius;
    out->sample_ticks = g_sample_ticks;
    return 0;
}

int thermal_emergency_active(void) {
    return g_emergency_active ? 1 : 0;
}
