#ifndef SSP2_LAB_H
#define SSP2_LAB_H

#include <stdint.h>

void ssp2__init(uint32_t max_clock_mhz);
uint8_t ssp2__exchange_byte(uint8_t data_out);

#endif // SSP2_LAB_H
