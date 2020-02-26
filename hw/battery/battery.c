#include "qemu/osdep.h"

#include "qemu/module.h"
#include "exec/address-spaces.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "hw/acpi/battery.h"
#include "trace.h"

#define TYPE_BATTERY_DEVICE "battery"  /* XXX */
#define BATTERY(obj) OBJECT_CHECK(BatteryState, (obj), TYPE_BATTERY_DEVICE)

typedef struct {
    DeviceState parent_obj;
    MemoryRegion mmio;

    uint64_t charge_now;
} BatteryState;

static uint64_t battery_mmio_read(void *opaque, hwaddr addr,
                                  unsigned size)
{
    BatteryState *s = BATTERY(opaque);
    trace_battery_mmio_read(addr, size, s->charge_now);
    return s->charge_now;
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
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static Property battery_properties[] = {
    DEFINE_PROP_UINT64("charge_now", BatteryState, charge_now, 1500),
    DEFINE_PROP_END_OF_LIST(),
};

static void battery_realize(DeviceState *dev, Error **errp)
{
    BatteryState *s = BATTERY(dev);

    /* XXX check that we are alone here */
    memory_region_init_io(&s->mmio, OBJECT(s), &battery_memory_ops, s,
        "battery-mmio", 16);
    memory_region_add_subregion(get_system_io(),
         BATTERY_ADDR_BASE, &s->mmio);
}

static void battery_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = battery_realize;
    device_class_set_props(dc, battery_properties);
    dc->user_creatable = true;
    dc->hotpluggable = false;

    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo battery_info = {
    .name = TYPE_BATTERY_DEVICE,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(BatteryState),
    .class_init = battery_class_init,
    .interfaces = (InterfaceInfo[]) {
    }
};

static void battery_register(void)
{
    type_register_static(&battery_info);
}

type_init(battery_register)
