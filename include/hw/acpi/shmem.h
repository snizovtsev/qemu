/*
 * shmem.h - ACPI shared memory definitions
 *
 * Copyright (C) 2020 Sergey Nizovtsev
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */
#ifndef HW_ACPI_SHMEM_H
#define HW_ACPI_SHMEM_H

#include "hw/qdev-core.h"
#include "hw/acpi/bios-linker-loader.h"

#define TYPE_ACPI_SHMEM          "acpi-shmem"
#define ACPI_SHMEM_MEM_FILE      "etc/acpi_shmem"
#define ACPI_SHMEM_ADDR_FILE     "etc/acpi_shmem_addr"
#define ACPI_SHMEM_MAX_SIZE      0x0fffffff

typedef struct AcpiShmemState AcpiShmemState;

#define ACPI_SHMEM(obj) \
    OBJECT_CHECK(AcpiShmemState, (obj), TYPE_ACPI_SHMEM)

void acpi_shmem_add_table(AcpiShmemState *s, GArray *table_data,
                          GArray *file_blob, BIOSLinker *linker);

void acpi_shmem_add_fw_cfg(AcpiShmemState *s, FWCfgState *fw_cfg);

static inline Object *find_acpi_shmem(void)
{
    bool ambig;
    Object *o = object_resolve_path_type("", TYPE_ACPI_SHMEM, &ambig);

    if (ambig || !o) {
        return NULL;
    }
    return o;
}

#endif /* HW_ACPI_SHMEM_H */
