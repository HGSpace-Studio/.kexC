#include <arch/smbios.h>
#include <arch/memory.h>
#include <string.h>

static smbios_bios_t* smbios_bios_entry = NULL;
static smbios_system_t* smbios_system_entry = NULL;
static smbios_memory_t* smbios_memory_entries[SMBIOS_MAX_ENTRIES];
static uint8_t smbios_memory_count = 0;
static uint8_t smbios_entry_count = 0;

void smbios_init(void)
{
    uint8_t* smbios_base = (uint8_t*)0xF0000;

    for (uint32_t i = 0; i < 0x10000; i += 16) {
        if (memcmp(smbios_base + i, "_SM_", 4) == 0) {
            uint8_t* entry = smbios_base + i + 16;
            smbios_entry_count = 0;

            while (entry < smbios_base + i + 0x1000) {
                smbios_header_t* header = (smbios_header_t*)entry;

                if (header->type == 0) {
                    smbios_bios_entry = (smbios_bios_t*)entry;
                } else if (header->type == 1) {
                    smbios_system_entry = (smbios_system_t*)entry;
                } else if (header->type == 17) {
                    if (smbios_memory_count < SMBIOS_MAX_ENTRIES) {
                        smbios_memory_entries[smbios_memory_count++] = (smbios_memory_t*)entry;
                    }
                }

                entry += header->length;
                while (entry[0] != 0 || entry[1] != 0) {
                    entry++;
                }
                entry += 2;

                smbios_entry_count++;
            }

            break;
        }
    }
}

smbios_bios_t* smbios_get_bios(void)
{
    return smbios_bios_entry;
}

smbios_system_t* smbios_get_system(void)
{
    return smbios_system_entry;
}

smbios_memory_t* smbios_get_memory(uint8_t index)
{
    if (index < smbios_memory_count) {
        return smbios_memory_entries[index];
    }
    return NULL;
}

uint8_t smbios_get_entry_count(void)
{
    return smbios_entry_count;
}
