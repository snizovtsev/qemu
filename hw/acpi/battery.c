/* Support for generating ACPI battery tables
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

void battery_build_acpi(BatteryIf *battery, Aml *table)
{
    Aml *method, *scope, *dev;

    if (1) {
        Aml *pcl = aml_package(2);
        aml_append(pcl, aml_name("\\_SB"));
        aml_append(pcl, aml_name("BAT0"));

        dev = aml_device("AC");
        aml_append(dev, aml_name_decl("_HID", aml_string("ACPI0003")));
        aml_append(dev, aml_name_decl("_UID", aml_int(0x00)));
        aml_append(dev, aml_name_decl("_PCL", pcl));

        method = aml_method("_PSR", 0, AML_NOTSERIALIZED);
        aml_append(method, aml_return(aml_int(0)));
        aml_append(dev, method);

        method = aml_method("_STA", 0, AML_NOTSERIALIZED);
        aml_append(method, aml_return(aml_int(0x0F)));
        aml_append(dev, method);

        scope = aml_scope("\\_SB");
        aml_append(scope, dev);
        aml_append(table, scope);
    }

    if (1) {
        Aml *pcl = aml_package(1);
        aml_append(pcl, aml_name("\\_SB"));

        dev = aml_device("BAT0");
        aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0C0A")));
        aml_append(dev, aml_name_decl("_UID", aml_int(0x00)));
        aml_append(dev, aml_name_decl("_PCL", pcl));

        aml_append(dev, aml_operation_region("CBAT", AML_SYSTEM_IO,
                                             aml_int(BATTERY_ADDR_BASE), 0x28));
        Aml *regs = aml_field("CBAT", AML_DWORD_ACC, AML_NOLOCK, AML_PRESERVE);
        aml_append(regs, aml_named_field("REG1", 32));
        aml_append(dev, regs);

        method = aml_method("_STA", 0, AML_NOTSERIALIZED);
        aml_append(method, aml_return(aml_int(0x1F)));
        aml_append(dev, method);

        /* Static battery information */
        method = aml_method("_BIF", 0, AML_NOTSERIALIZED);
        Aml *bif = aml_package(0xd);
        /* 0x00, Power Unit (0=mWh/mW, 1=mAh/mA) */
        aml_append(bif, aml_int(0x01));
        /* 0x01, Design Capacity (mWh/mAh), charge_full_design/1000 */
        aml_append(bif, aml_int(7236));
        /* 0x02, Last Full Charge Capacity (mWh/mAh), charge_full/1000 */
        aml_append(bif, aml_int(3006));
        /* 0x03, Battery technology (0=Primary-Non-Recharge,1=Secondry-rechange) */
        aml_append(bif, aml_int(0x01));
        /* Field 0x04, Design voltage (mV), voltage_min_design/1000 */
        aml_append(bif, aml_int(7600));
        /* 0x05, Design capacity of warning (mWh/mAh) */
        aml_append(bif, aml_int(400));
        /* 0x06, Design capacity of low (mWh/mAh) */
        aml_append(bif, aml_int(200));
        /* 0x07, Battery capacity gradularity 1 (mWh/mAh) */
        aml_append(bif, aml_int(0x04));
        /* 0x08, Battery capacity gradularity 2 (mWh/mAh) */
        aml_append(bif, aml_int(0x04));
        /* 0x09, Model number, model_name */
        aml_append(bif, aml_string("Virtual ACPI Battery"));
        /* 0x0a, Serial number, serial_number */
        aml_append(bif, aml_string("00000"));
        /* 0x0b, Battery type, technology */
        aml_append(bif, aml_string("LI-ION"));
        /* 0x0c, OEM Information, manufacturer */
        aml_append(bif, aml_string("QEMU"));
        aml_append(method, aml_return(bif));
        aml_append(dev, method);

        /* Runtime battery status */
        method = aml_method("_BST", 0, AML_NOTSERIALIZED);
        Aml *bst = aml_package(0x4);
        /* 0x00, Battery state, status */
        aml_append(bst, aml_int(0x1)); // discharging
        /* 0x01, Battery present rate (mW/mA), current_now/1000 */
        aml_append(bst, aml_int(1000));
        /* 0x02, Battery remaining capacity (mWh/mAh), charge_now/1000 */
        aml_append(bst, aml_name("REG1"));
        //aml_append(bst, aml_int(500));
        /* 0x03, Battery present voltage (mV), voltage_now */
        aml_append(bst, aml_int(8325));
        aml_append(method, aml_return(bst));
        aml_append(dev, method);

        scope = aml_scope("\\_SB");
        aml_append(scope, dev);
        aml_append(table, scope);
    }
}
