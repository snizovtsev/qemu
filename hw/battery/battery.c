/*
 * QEMU battery backend
 *
 * Copyright (C) 2020 Sergey Nizovtsev
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"

#include "qemu/module.h"
#include "exec/address-spaces.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "hw/acpi/battery.h"
#include "hw/acpi/acpi_dev_interface.h"
#include "trace.h"

#define BATTERY(obj) OBJECT_CHECK(BatteryState, (obj), TYPE_BATTERY_DEVICE)

typedef struct {
    DeviceState parent_obj;
    MemoryRegion mmio;
    uint32_t regs[BATTERY_R_MAX];
} BatteryState;

static uint64_t battery_mmio_read(void *opaque, hwaddr addr,
                                  unsigned size)
{
    BatteryState *s = BATTERY(opaque);
    uint32_t val = BATTERY_UNKNOWN;

    if (addr <= BATTERY_A_MAX) {
        val = s->regs[addr / 4];
    }

    trace_battery_mmio_read(addr, size, val);
    return val;
}

static void battery_mmio_write(void *opaque, hwaddr addr,
                               uint64_t val, unsigned size)
{
    /* should not be called */
    trace_battery_mmio_write(addr, size, val);
}

static const MemoryRegionOps battery_memory_ops = {
    .read = battery_mmio_read,
    .write = battery_mmio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static void battery_get_register(Object *obj, Visitor *v,
                                 const char *name, void *opaque,
                                 Error **errp)
{
    BatteryState *s = BATTERY(obj);
    uintptr_t i = (uintptr_t) opaque;
    uint32_t *reg;

    reg = &s->regs[i];
    visit_type_uint32(v, name, reg, errp);
}

static void battery_set_register(Object *obj, Visitor *v,
                                 const char *name, void *opaque,
                                 Error **errp)
{
    Object *acpidev = object_resolve_path_type("", TYPE_ACPI_DEVICE_IF, NULL);
    BatteryState *s = BATTERY(obj);
    uintptr_t i = (uintptr_t) opaque;
    Error *local_err = NULL;
    uint32_t *reg, val;

    visit_type_uint32(v, name, &val, &local_err);
    if (local_err) {
        goto out;
    }

    reg = &s->regs[i];
    *reg = val;

    if (acpidev && s->parent_obj.realized) {
        acpi_send_event(DEVICE(acpidev), ACPI_BATTERY_STATUS);
    }

out:
    error_propagate(errp, local_err);
}

static Property battery_properties[] = {
    DEFINE_PROP_UINT32("voltage_min_design", BatteryState,
                       regs[R_BATTERY_VOLTAGE_MIN_DESIGN],
                       10000 /* 10V */),
    DEFINE_PROP_UINT32("charge_full_design", BatteryState,
                       regs[R_BATTERY_CHARGE_FULL_DESIGN],
                       8000),
    DEFINE_PROP_UINT32("charge_full", BatteryState,
                       regs[R_BATTERY_CHARGE_FULL],
                       6000),
    DEFINE_PROP_UINT32("cycle_count", BatteryState,
                       regs[R_BATTERY_CYCLE_COUNT],
                       500),
    DEFINE_PROP_END_OF_LIST(),
};

static void battery_realize(DeviceState *dev, Error **errp)
{
    BatteryState *s = BATTERY(dev);
    /* TODO check that we are alone here */

    memory_region_init_io(&s->mmio, OBJECT(s), &battery_memory_ops, s,
        "battery-mmio", BATTERY_MMIO_LEN);
    memory_region_add_subregion(get_system_memory(),
         BATTERY_MMIO_BASE, &s->mmio);
}

static void battery_class_property_add_register(ObjectClass *klass,
                                                const char *name,
                                                size_t reg)
{
    object_class_property_add(klass, name, "uint32",
                              battery_get_register,
                              battery_set_register,
                              NULL, /* release */
                              (void*) reg,
                              NULL);
}

static void battery_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = battery_realize;
    dc->user_creatable = true;
    dc->hotpluggable = false;

    /* Read-only properties */
    device_class_set_props(dc, battery_properties);

    /* Writable properties */
    battery_class_property_add_register(klass, "state",
                                        R_BATTERY_STATE);
    battery_class_property_add_register(klass, "voltage_now",
                                        R_BATTERY_VOLTAGE_NOW);
    battery_class_property_add_register(klass, "current_now",
                                        R_BATTERY_CURRENT_NOW);
    battery_class_property_add_register(klass, "charge_now",
                                        R_BATTERY_CHARGE_NOW);

    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static void battery_register(void)
{
    static const TypeInfo battery_info = {
        .name = TYPE_BATTERY_DEVICE,
        .parent = TYPE_DEVICE,
        .instance_size = sizeof(BatteryState),
        .class_init = battery_class_init,
    };

    type_register_static(&battery_info);
}

type_init(battery_register)
