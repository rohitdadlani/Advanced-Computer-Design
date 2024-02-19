#include "gpio_lab.h"
#include "LPC40xx.h"

void gpio0__set_as_input(uint8_t pin_num) {
  LPC_GPIO0->DIR &= ~(1 << pin_num);
}

void gpio0__set_as_output(uint8_t pin_num) {
  LPC_GPIO0->DIR |= (1 << pin_num);
}

void gpio0__set_high(uint8_t pin_num) {
  LPC_GPIO0->PIN |= (1 << pin_num);
}

void gpio0__set_low(uint8_t pin_num) {
  LPC_GPIO0->PIN &= ~(1 << pin_num);
}

void gpio0__set(uint8_t pin_num, bool high) {
  if (high) {
    gpio0__set_high(pin_num);
  } else {
    gpio0__set_low(pin_num);
  }
}

bool gpio0__get_level(uint8_t pin_num) {
  return LPC_GPIO0->PIN & (1 << pin_num);
}
