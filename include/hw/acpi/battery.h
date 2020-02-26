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

#include "hw/acpi/aml-build.h"

typedef void* BatteryIf; // XXX

#define BATTERY_ADDR_BASE          0x4040

void battery_build_acpi(BatteryIf *battery, Aml *dev);

#endif // HW_ACPI_BATTERY_H
