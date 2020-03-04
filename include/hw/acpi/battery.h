/*
 * battery.h - Laptop battery ACPI definitions
 *
 * Copyright (C) Sergey Nizovtsev
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */
#ifndef HW_ACPI_BATTERY_H
#define HW_ACPI_BATTERY_H

#include "hw/registerfields.h"
#include "hw/acpi/aml-build.h"

#define TYPE_BATTERY_DEVICE "battery"
#define BATTERY_UNKNOWN            0xFFFFFFFF

#define BATTERY_MMIO_BASE          0xFEF40000
#define BATTERY_MMIO_LEN           0x00001000

REG32(BATTERY_STATE, 0x00)
  FIELD(BATTERY_STATE, discharging, 0, 1)
  FIELD(BATTERY_STATE, charging, 1, 1)
REG32(BATTERY_VOLTAGE_NOW, 0x04)
REG32(BATTERY_CURRENT_NOW, 0x08)
REG32(BATTERY_CHARGE_NOW, 0x0C)
REG32(BATTERY_VOLTAGE_MIN_DESIGN, 0x30)
REG32(BATTERY_CHARGE_FULL_DESIGN, 0x34)
REG32(BATTERY_CHARGE_FULL, 0x38)
REG32(BATTERY_CYCLE_COUNT, 0x3C)

#define BATTERY_A_MAX A_BATTERY_CHARGE_FULL
#define BATTERY_R_MAX R_BATTERY_CHARGE_FULL

void battery_build_acpi(Aml *scope);

#endif // HW_ACPI_BATTERY_H
