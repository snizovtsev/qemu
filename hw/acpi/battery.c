/* Support for generating ACPI control method battery device
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
#include "hw/acpi/battery.h"

#define BATTERY_AC_PSR_OFFLINE     0x0
#define BATTERY_AC_PSR_ONLINE      0x1

static Aml *aml_battery_regs(const char *prefix, const char *region)
{
    Aml *regs;
    char name[5];
    int i;

    assert(strlen(prefix) == 2);
    assert(BATTERY_R_MAX < 100);

    name[0] = prefix[0];
    name[1] = prefix[1];
    name[4] = '\0';

    regs = aml_field(region, AML_DWORD_ACC, AML_NOLOCK, AML_PRESERVE);
    for (i = 0; i <= BATTERY_R_MAX; ++i) {
        name[2] = i / 10 + '0';
        name[3] = i % 10 + '0';
        aml_append(regs, aml_named_field(name, 32));
    }

    return regs;
}

static Aml *aml_ac_adapter(/* const char **rstate */void)
{
    Aml *dev, *method, *pcl/* , *block */;

    dev = aml_device("AC");
    aml_append(dev, aml_name_decl("_HID", aml_string("ACPI0003")));
    aml_append(dev, aml_name_decl("_UID", aml_int(0x1)));

    /* Power Consumer List */
    pcl = aml_package(1);
    aml_append(pcl, aml_name("\\_SB")); /* System Bus */
    aml_append(dev, aml_name_decl("_PCL", pcl));

    /* Device status */
    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0xf)));
    aml_append(dev, method);

    /* Power Source */
    method = aml_method("_PSR", 0, AML_NOTSERIALIZED);

    // new logic: if at least one battery charging then we have external power
#if 0
    /* Local0 = CBRI(STATUS); */
    aml_append(method, aml_store(
                   aml_call1("CBRI", aml_int(BATTERY_PROP_STATUS)),
                   aml_local(0)));
    /* If (status == discharging) return offline; */
    block = aml_if(aml_equal(aml_local(0),
                             aml_int(BATTERY_STATUS_DISCHARGING)));
    aml_append(block, aml_return(aml_int(BATTERY_AC_PSR_OFFLINE)));
    aml_append(method, block);

#endif

    aml_append(method, aml_return(aml_int(BATTERY_AC_PSR_OFFLINE)));
    aml_append(dev, method);

    return dev;
}

/* Static battery information */
static Aml* aml_battery_bif_package(const char *regs)
{
    Aml *pkg = aml_package(0xd);

    aml_append(pkg, /* 0x00, Power Unit: mAh/mA */
               aml_int(0x01));
    aml_append(pkg, /* 0x01, Design Capacity (mAh) */
               aml_name("%s%02d", regs, R_BATTERY_CHARGE_FULL_DESIGN));
    aml_append(pkg, /* 0x02, Last Full Charge Capacity (mAh) */
               aml_name("%s%02d", regs, R_BATTERY_CHARGE_FULL));
    aml_append(pkg, /* 0x03, Battery technology */
               aml_int(0x01 /* Rechargable */));
    aml_append(pkg, /* 0x04, Design voltage (mV) */
               aml_name("%s%02d", regs, R_BATTERY_VOLTAGE_MIN_DESIGN));
    aml_append(pkg, /* 0x05, Design capacity of warning (mAh) */
               aml_int(BATTERY_UNKNOWN));
    aml_append(pkg, /* 0x06, Design capacity of low (mAh) */
               aml_int(BATTERY_UNKNOWN));
    aml_append(pkg, /* 0x07, Capacity granularity between low and warning */
               aml_int(0x01 /* mAh */));
    aml_append(pkg, /* 0x08, Capacity granularity between warning and full */
               aml_int(0x01 /* mAh */));
    aml_append(pkg, /* 0x09, Model number, model_name */
               aml_string("Virtual Battery"));
    aml_append(pkg, /* 0x0a, Serial number, serial_number */
               aml_string("00000"));
    aml_append(pkg, /* 0x0b, Battery type, technology */
               aml_string("LI-ION"));
    aml_append(pkg, /* 0x0c, OEM Information, manufacturer */
               aml_string("QEMU"));

    return pkg;
}

/* Runtime battery status */
static Aml* aml_battery_bst_package(const char *regs)
{
    Aml *pkg = aml_package(0x4);

    aml_append(pkg, /* 0x00, Battery state, status */
               aml_name("%s%02d", regs, R_BATTERY_STATE)); /* TODO: handle low levels */
    aml_append(pkg, /* 0x01, Battery present rate (mA) */
               aml_name("%s%02d", regs, R_BATTERY_CURRENT_NOW));
    aml_append(pkg, /* 0x02, Battery remaining capacity (mAh) */
               aml_name("%s%02d", regs, R_BATTERY_CHARGE_NOW));
    aml_append(pkg, /* 0x03, Battery present voltage (mV) */
               aml_name("%s%02d", regs, R_BATTERY_VOLTAGE_NOW));

    return pkg;
}

static Aml* aml_battery(int id, const char *regs)
{
    Aml *method, *pcl, *dev;

    dev = aml_device("BAT%d", id);
    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0C0A")));
    aml_append(dev, aml_name_decl("_UID", aml_int(id)));
    pcl = aml_package(1);
    aml_append(pcl, aml_name("\\_SB"));
    aml_append(dev, aml_name_decl("_PCL", pcl));

    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, /* bits 0-3 represent a battery bay and always set;
                        * bit 4 set if battery present */
               aml_return(aml_int(0x1f)));
    aml_append(dev, method);

    method = aml_method("_BIF", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_battery_bif_package(regs)));
    aml_append(dev, method);

    method = aml_method("_BST", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_battery_bst_package(regs)));
    aml_append(dev, method);

    return dev;
}

void battery_build_acpi(BatteryIf *battery, Aml *table)
{
    Aml *scope = aml_scope("\\_SB");

    aml_append(scope, aml_operation_region("CBAT", AML_SYSTEM_MEMORY,
                                           aml_int(BATTERY_MMIO_BASE),
                                           BATTERY_MMIO_LEN));
    aml_append(scope, aml_battery_regs("BR", "CBAT"));
    aml_append(scope, aml_ac_adapter());
    aml_append(scope, aml_battery(0, "BR"));

    aml_append(table, scope);
}
