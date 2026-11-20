/* crc32.h —— 标准 CRC-32（多项式 0xEDB88320）实现，用于快照完整性校验。 */
#ifndef NAV_CRC32_H
#define NAV_CRC32_H

#include <stddef.h>
#include <stdint.h>

uint32_t nav_crc32(const void *data, size_t len);
uint32_t nav_crc32_update(uint32_t crc, const void *data, size_t len);

#endif /* NAV_CRC32_H */
