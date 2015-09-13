#include <stdint.h>   
#include <stddef.h>

uint32_t crc_update(uint32_t crc, uint8_t *abuf, size_t len);
uint32_t crc(uint8_t *abuf, size_t len);
uint32_t crc_init(uint8_t *abuf, size_t len);
uint32_t crc_final(uint32_t crc);
