/* crc32.c —— CRC-32 查表实现（IEEE 802.3，反射型，初值 0xFFFFFFFF）。 */
#include "crc32.h"

static uint32_t s_table[256];
static int s_table_ready = 0;

static void crc32_init_table(void)
{
    uint32_t i;
    for (i = 0; i < 256; ++i) {
        uint32_t c = i;
        int k;
        for (k = 0; k < 8; ++k) {
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        s_table[i] = c;
    }
    s_table_ready = 1;
}

uint32_t nav_crc32_update(uint32_t crc, const void *data, size_t len)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t i;

    if (!s_table_ready) {
        crc32_init_table();
    }
    crc = crc ^ 0xFFFFFFFFu;
    for (i = 0; i < len; ++i) {
        crc = s_table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

uint32_t nav_crc32(const void *data, size_t len)
{
    return nav_crc32_update(0u, data, len);
}
