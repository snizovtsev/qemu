#include "qemu/osdep.h"

#include "qemu/event_notifier.h"
#include "qemu/memfd.h"
#include "chardev/char-fe.h"
#include "sysemu/reset.h"
#include "qapi/error.h"
#include "hw/acpi/acpi.h"
#include "hw/acpi/aml-build.h"
#include "hw/acpi/shmem.h"
#include "hw/qdev-properties.h"
#include "hw/nvram/fw_cfg.h"

typedef struct AcpiHelperResponse AcpiHelperResponse;

struct AcpiHelperResponse {
    int mem_fd;
    int irq_fd;
    int doorbell_fd;
    size_t mem_size;
};

struct AcpiShmemState {
    DeviceState parent_obj;
    bool enable_battery;
    MemoryRegion mr;
    CharBackend helper_chr;
    EventNotifier irq;
    EventNotifier doorbell;
    uint8_t file_addr_le[8];
};

void acpi_shmem_add_table(AcpiShmemState *s, GArray *table_data,
                          GArray *file_blob, BIOSLinker *linker)
{
    Aml *ssdt;
    int patch_addr_offset;

    g_array_set_size(file_blob, memory_region_size(&s->mr));
    bios_linker_loader_alloc(linker, ACPI_SHMEM_MEM_FILE,
                             file_blob, sizeof(uint64_t), false);

    ssdt = init_aml_allocator();

    acpi_data_push(ssdt->buf, sizeof(AcpiTableHeader));
    patch_addr_offset = table_data->len +
        build_append_named_dword(ssdt->buf, "%s", "BPTR");

    /* TODO: construct a proxy method for event handling (GED/GPE) */

    if (s->enable_battery) {
        /* Generate cmbattery code */
    }

    g_array_append_vals(table_data, ssdt->buf->data, ssdt->buf->len);

    bios_linker_loader_add_pointer(linker,
        ACPI_BUILD_TABLE_FILE, patch_addr_offset, sizeof(uint32_t),
        ACPI_SHMEM_MEM_FILE, 0);

    bios_linker_loader_write_pointer(linker,
        ACPI_SHMEM_ADDR_FILE, 0, sizeof(uint64_t),
        ACPI_SHMEM_MEM_FILE, 0);

    build_header(linker, table_data,
        (void *)(table_data->data + table_data->len - ssdt->buf->len),
        "SSDT", ssdt->buf->len, 1, NULL, "VMBoard");

    free_aml_allocator();
}

static void acpi_shmem_remap(AcpiShmemState *s)
{
    MemoryRegion *mr = &s->mr;
    uint64_t new_offset;

    memcpy(&new_offset, s->file_addr_le, sizeof(uint64_t));

    if (memory_region_is_mapped(&s->mr)) {
        if (new_offset != 0) {
            memory_region_set_address(mr, (hwaddr) new_offset);
        } else {
            memory_region_del_subregion(get_system_memory(), &s->mr);
        }
    } else {
        if (new_offset != 0) {
            memory_region_add_subregion_overlap(
                get_system_memory(), (hwaddr) new_offset, mr, 1);
        }
    }
}

static void acpi_shmem_addr_cb(void *opaque, off_t start, size_t len)
{
    acpi_shmem_remap((AcpiShmemState *) opaque);
}

static void acpi_shmem_reset_cb(void *opaque)
{
    AcpiShmemState *s = opaque;

    /* Unmap old region on reboot to avoid clashing with firmware. */
    memset(s->file_addr_le, 0, ARRAY_SIZE(s->file_addr_le));
    acpi_shmem_remap(s);
}

void acpi_shmem_add_fw_cfg(AcpiShmemState *s, FWCfgState *fw_cfg)
{
    void *ram_ptr = memory_region_get_ram_ptr(&s->mr);
    size_t ram_size = memory_region_size(&s->mr);

    fw_cfg_add_file_callback(fw_cfg, ACPI_SHMEM_MEM_FILE, NULL, NULL, NULL,
        ram_ptr, ram_size, true /* read only */);

    fw_cfg_add_file_callback(fw_cfg, ACPI_SHMEM_ADDR_FILE,
        NULL, acpi_shmem_addr_cb, s,
        s->file_addr_le, ARRAY_SIZE(s->file_addr_le), false);

    qemu_register_reset(acpi_shmem_reset_cb, s);
}

static int acpi_shmem_recvmsg(CharBackend *chr, Error **errp)
{
    uint8_t data;
    int ret;

    do {
        ret = qemu_chr_fe_read_all(chr, &data, 1);
        if (ret < 0) {
            if (ret == -EINTR) {
                continue;
            }
            error_setg_errno(errp, -ret, "read from helper failed");
            return -1;
        } else if (ret == 0) {
            error_setg_errno(errp, -ret, "helper shutdown early");
            return -1;
        }
    } while (ret <= 0);

    ret = qemu_chr_fe_get_msgfd(chr);
    if (ret < 0) {
        error_setg(errp, "no msgfd found in chardev");
        return -1;
    }

    return ret;
}

static void acpi_shmem_interact(AcpiShmemState *s, AcpiHelperResponse *resp,
                                Error **errp)
{
    Error *err = NULL;
    int fd[] = {-1, -1, -1};
    struct stat meta;

    for (unsigned i = 0; i < ARRAY_SIZE(fd); ++i) {
        fd[i] = acpi_shmem_recvmsg(&s->helper_chr, &err);
        if (err) {
            goto fail;
        }
    }

    if (fstat(fd[0], &meta) < 0) {
        error_setg_errno(&err, errno,
            "can't determine size of shared memory sent by helper");
        goto fail;
    }
    if (meta.st_size < 8 || meta.st_size > ACPI_SHMEM_MAX_SIZE) {
        error_setg(&err, "bad size of shared memory: %zu", meta.st_size);
        goto fail;
    }

    if (resp) {
        resp->mem_fd = fd[0];
        resp->irq_fd = fd[1];
        resp->doorbell_fd = fd[2];
        resp->mem_size = meta.st_size;
    }
    return;

 fail:
    error_propagate(errp, err);
    for (unsigned i = 0; i < ARRAY_SIZE(fd); ++i) {
        if (fd[i] >= 0) {
            close(fd[i]);
        }
    }
}

static void acpi_shmem_irq_cb(EventNotifier *irq)
{
    /* XXX: make it link property */
    Object *event_dev = object_resolve_path_type("", TYPE_ACPI_DEVICE_IF, NULL);

    /* XXX: argue about why not moving it to bottom */
    event_notifier_test_and_clear(irq);

    /* Notify ACPI devices using GPE or GED */
    acpi_send_event(DEVICE(event_dev), ACPI_SHMEM_NOTIFY_STATUS);
}

static void acpi_shmem_realize(DeviceState *dev, Error **errp)
{
    AcpiShmemState *s = ACPI_SHMEM(dev);
    AcpiHelperResponse resp;
    Error *err = NULL;

    /* Given that this function is executing, there is at least one shmem
     * device. Check if there are several.
     */
    if (!find_acpi_shmem()) {
        error_setg(errp, "at most one %s device is permitted",
                   TYPE_ACPI_SHMEM);
        return;
    }

    if (!qemu_chr_fe_backend_connected(&s->helper_chr)) {
        error_setg(errp, "you must specify a 'chardev'");
        return;
    }

    acpi_shmem_interact(s, &resp, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    memory_region_init_ram_from_fd(&s->mr, OBJECT(s), "acpi-shmem",
                                   resp.mem_size, true, resp.mem_fd, &err);
    if (err) {
        error_propagate(errp, err);
        return;
    }

    event_notifier_init_fd(&s->irq, resp.irq_fd);
    event_notifier_init_fd(&s->doorbell, resp.doorbell_fd);

    memory_region_add_eventfd(&s->mr, 0, 4, false, 0, &s->doorbell);
    event_notifier_set_handler(&s->irq, acpi_shmem_irq_cb);
}

static Property acpi_shmem_properties[] = {
    DEFINE_PROP_CHR("chardev", AcpiShmemState, helper_chr),
    DEFINE_PROP_BOOL("battery", AcpiShmemState, enable_battery, true),
    /* TODO: r/w option to reconnect on reboot? */
    DEFINE_PROP_END_OF_LIST(),
};

static void acpi_shmem_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->hotpluggable = false;
    dc->realize = acpi_shmem_realize;
    device_class_set_props(dc, acpi_shmem_properties);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo acpi_shmem_info = {
    .name          = TYPE_ACPI_SHMEM,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(AcpiShmemState),
    .class_init    = acpi_shmem_class_init,
};

static void acpi_shmem_register_types(void)
{
    type_register_static(&acpi_shmem_info);
}

type_init(acpi_shmem_register_types);
