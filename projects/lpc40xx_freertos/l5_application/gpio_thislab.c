#include "gpio_thislab.h"
#include "lpc40xx.h"
#include "lpc_peripherals.h"
#include "stdint.h"
#include <stdio.h>

static function_pointer_t gpio0_callbacks[32];

void gpio0__attach_interrupt(uint32_t pin, gpio_interrupt_e interrupt_type, function_pointer_t callback) {

  gpio0_callbacks[pin] = callback;
  if (interrupt_type == GPIO_INTR__RISING_EDGE) {
    LPC_GPIOINT->IO0IntEnR |= (1 << pin);
  } else {
    LPC_GPIOINT->IO0IntEnF |= (1 << pin);
  }
}
void gpio0__interrupt_dispatcher(void) {

  uint32_t interrupt_status = LPC_GPIOINT->IO0IntStatF | LPC_GPIOINT->IO0IntStatR;

  for (uint8_t i = 0; i < 32; i++) {
    if (interrupt_status & (1 << i)) {

      function_pointer_t attached_user_handler = gpio0_callbacks[i];
      attached_user_handler();
      clear_pin_interrupt(i);
    }
  }
}
void clear_pin_interrupt(uint32_t pin) { LPC_GPIOINT->IO0IntClr |= (1 << pin); }
