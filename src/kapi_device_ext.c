#include "kapi_device_ext.h"
#include "kapi.h"
#include <string.h>

#define KAPI_DEV_TABLE_MAX  256

static kapi_device_info_t dev_table[KAPI_DEV_TABLE_MAX];
static kapi_pci_info_t pci_table[64];
static int pci_count = 0;
static kapi_usb_info_t usb_table[64];
static int usb_count = 0;
static kapi_fb_info_t fb_table[8];
static int fb_count = 0;

int kapi_device_ext_init(void)
{
    memset(dev_table, 0, sizeof(dev_table));
    memset(pci_table, 0, sizeof(pci_table));
    memset(usb_table, 0, sizeof(usb_table));
    memset(fb_table, 0, sizeof(fb_table));
    pci_count = 0;
    usb_count = 0;
    fb_count = 0;
    return KAPI_OK;
}

int kapi_dev_register(const kapi_device_info_t* info)
{
    if (!info) return KAPI_EINVAL;
    for (int i = 0; i < KAPI_DEV_TABLE_MAX; i++) {
        if (dev_table[i].id == 0) {
            dev_table[i] = *info;
            dev_table[i].id = (kapi_dev_id_t)(i + 1);
            dev_table[i].state = KAPI_DEV_STATE_ACTIVE;
            return (int)dev_table[i].id;
        }
    }
    return KAPI_ENOMEM;
}

int kapi_dev_unregister(kapi_dev_id_t id)
{
    for (int i = 0; i < KAPI_DEV_TABLE_MAX; i++) {
        if (dev_table[i].id == id) {
            memset(&dev_table[i], 0, sizeof(dev_table[i]));
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

kapi_device_info_t* kapi_dev_find_by_id(kapi_dev_id_t id)
{
    for (int i = 0; i < KAPI_DEV_TABLE_MAX; i++) {
        if (dev_table[i].id == id) return &dev_table[i];
    }
    return NULL;
}

kapi_device_info_t* kapi_dev_find_by_name(const char* name)
{
    if (!name) return NULL;
    for (int i = 0; i < KAPI_DEV_TABLE_MAX; i++) {
        if (dev_table[i].id && strcmp(dev_table[i].name, name) == 0)
            return &dev_table[i];
    }
    return NULL;
}

int kapi_dev_list(kapi_device_info_t* out, int max, int* count)
{
    if (!out || !count) return KAPI_EINVAL;
    int n = 0;
    for (int i = 0; i < KAPI_DEV_TABLE_MAX && n < max; i++) {
        if (dev_table[i].id) {
            out[n++] = dev_table[i];
        }
    }
    *count = n;
    return KAPI_OK;
}

int kapi_dev_set_state(kapi_dev_id_t id, int state)
{
    kapi_device_info_t* d = kapi_dev_find_by_id(id);
    if (!d) return KAPI_ENOENT;
    d->state = state;
    return KAPI_OK;
}

int kapi_pci_enumerate(void)
{
    return pci_count;
}

int kapi_pci_get_info(int index, kapi_pci_info_t* info)
{
    if (!info || index < 0 || index >= pci_count) return KAPI_EINVAL;
    *info = pci_table[index];
    return KAPI_OK;
}

int kapi_pci_list(kapi_pci_info_t* out, int max)
{
    if (!out) return KAPI_EINVAL;
    int n = pci_count < max ? pci_count : max;
    memcpy(out, pci_table, (size_t)n * sizeof(kapi_pci_info_t));
    return n;
}

int kapi_pci_enable_device(kapi_pci_info_t* pci)
{
    if (!pci) return KAPI_EINVAL;
    pci->is_enabled = true;
    return KAPI_OK;
}

int kapi_pci_disable_device(kapi_pci_info_t* pci)
{
    if (!pci) return KAPI_EINVAL;
    pci->is_enabled = false;
    return KAPI_OK;
}

int kapi_pci_set_master(kapi_pci_info_t* pci)
{
    if (!pci) return KAPI_EINVAL;
    pci->is_bus_master = true;
    pci->command |= 0x0004;
    return KAPI_OK;
}

void* kapi_pci_map_bar(kapi_pci_info_t* pci, int bar)
{
    if (!pci || bar < 0 || bar >= 6) return NULL;
    if (pci->mmio_base[bar]) return pci->mmio_base[bar];
    if (pci->bar[bar] & 0x01) {
        return (void*)(uintptr_t)(pci->bar[bar] & ~0x03u);
    }
    return (void*)(uintptr_t)pci->bar[bar];
}

int kapi_usb_enumerate(void)
{
    return usb_count;
}

int kapi_usb_get_info(int index, kapi_usb_info_t* info)
{
    if (!info || index < 0 || index >= usb_count) return KAPI_EINVAL;
    *info = usb_table[index];
    return KAPI_OK;
}

int kapi_fb_get_info(int index, kapi_fb_info_t* info)
{
    if (!info || index < 0 || index >= fb_count) return KAPI_EINVAL;
    *info = fb_table[index];
    return KAPI_OK;
}

int kapi_fb_count(void)
{
    return fb_count;
}