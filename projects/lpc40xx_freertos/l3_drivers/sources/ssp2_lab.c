#include "ssp2_lab.h"
#include "lpc40xx.h"
//#include "system_LPC40xx.h" // This might vary based on your specific microcontroller

#define SystemCoreClock 48000000

void ssp2__init(uint32_t max_clock_mhz) {
  // a) Power on Peripheral: SSP2
  LPC_SC->PCONP |= (1 << 20);

  // b) Setup control registers CR0 and CR1
  LPC_SSP2->CR0 = 7;        // 8-bit mode
  LPC_SSP2->CR1 = (1 << 1); // Enable SSP as Master

  // c) Setup prescalar register to be <= max_clock_mhz
  uint32_t prescaler = (SystemCoreClock / (max_clock_mhz * 1000000)) & 0xFFFE; // Ensure it's even
  LPC_SSP2->CPSR = prescaler;
}

uint8_t ssp2__exchange_byte(uint8_t data_out) {
  // Ensure SSP is not busy and TX FIFO not full before writing
  while (LPC_SSP2->SR & (1 << 4))
    ; // Wait until not busy
  LPC_SSP2->DR = data_out;

  // Wait until data is received
  while (LPC_SSP2->SR & (1 << 4))
    ; // Wait until not busy

  return (uint8_t)LPC_SSP2->DR; // Read received data
}
