#include <arch/acpi.h>
#include <arch/memory.h>
#include <string.h>

static acpi_header_t* acpi_tables[ACPI_MAX_TABLES];
static uint8_t acpi_table_count = 0;
static mcfg_t* acpi_mcfg = NULL;

static uint8_t acpi_checksum(uint8_t* data, uint32_t length)
{
    uint8_t sum = 0;
    for (uint32_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

void acpi_init(void)
{
    uint8_t* ebda = (uint8_t*)0x40E;
    uint32_t ebda_base = *(uint16_t*)ebda << 4;

    for (uint32_t addr = ebda_base; addr < ebda_base + 0x400; addr += 16) {
        if (addr < 0x1000) {
            continue;
        }

        uint8_t* ptr = (uint8_t*)addr;
        if (memcmp(ptr, "RSD PTR ", 8) == 0) {
            rsdt_t* rsdt = (rsdt_t*)(ptr + 16);

            uint32_t* tables = (uint32_t*)((uint8_t*)rsdt + 36);
            uint32_t length = rsdt->header.length;
            uint32_t count = (length - 36) / 4;

            for (uint32_t i = 0; i < count && acpi_table_count < ACPI_MAX_TABLES; i++) {
                acpi_header_t* table = (acpi_header_t*)tables[i];
                if (table && acpi_checksum((uint8_t*)table, table->length) == 0) {
                    acpi_tables[acpi_table_count++] = table;

                    if (table->signature == 0x4346474D) {
                        acpi_mcfg = (mcfg_t*)table;
                    }
                }
            }

            break;
        }
    }
}

acpi_header_t* acpi_get_table(uint32_t signature)
{
    for (uint8_t i = 0; i < acpi_table_count; i++) {
        if (acpi_tables[i]->signature == signature) {
            return acpi_tables[i];
        }
    }
    return NULL;
}

uint8_t acpi_get_table_count(void)
{
    return acpi_table_count;
}

uint32_t acpi_get_lmbr_base(void)
{
    if (acpi_mcfg) {
        return acpi_mcfg->lmbr_base;
    }
    return 0;
}

uint8_t acpi_get_lmbr_size(void)
{
    if (acpi_mcfg) {
        return acpi_mcfg->lmbr_size;
    }
    return 0;
}
