#include <stdio.h>

#include "FreeRTOS.h"
#include "LPC40xx.h"
#include "gpio_lab.h"

#include "task.h"

#include "board_io.h"
#include "common_macros.h"
#include "periodic_scheduler.h"
#include "sj2_cli.h"

// 'static' to make these functions 'private' to this file
static void create_blinky_tasks(void);
static void create_uart_task(void);
static void blink_task(void *params);
static void uart_task(void *params);

typedef struct {
  uint8_t port;
  uint8_t pin;
} port_pin_s;

void led_task(void *pvParameters) {

  LPC_IOCON->P1_18 &= ~0x07;   // Set IOCON MUX function to 000
  LPC_GPIO1->DIR |= (1 << 18); // Set DIR register bit for LED port pin to output

  while (true) {
    LPC_GPIO1->PIN &= ~(1 << 18); // Set PIN register bit to 0 to turn ON LED (active low)
    vTaskDelay(500);

    LPC_GPIO1->PIN |= (1 << 18); // Set PIN register bit to 1 to turn OFF LED
    vTaskDelay(500);
  }

  const port_pin_s *led = (port_pin_s *)(pvParameters);

  gpio0__set_as_output(led->pin); // Set the LED GPIO as output

  while (true) {
    gpio0__set_high(led->pin);
    vTaskDelay(100);
    gpio0__set_low(led->pin);
    vTaskDelay(100);
  }

  /*
   // Choose one of the onboard LEDS by looking into schematics and write code for the below
   LPC_IOCON->P2_0 &= ~0x07;

   LPC_GPIO2->DIR |= (1 << 0);

   while (true) {
      LPC_GPIO2->PIN &= ~(1 << 0);
      vTaskDelay(500);
 




      LPC_GPIO2->PIN |= (1 << 0);
      vTaskDelay(500);
  }
  */
}

int main(void) {
  create_blinky_tasks();
  create_uart_task();

  xTaskCreate(led_task, "led", 2048 / sizeof(void *), NULL, PRIORITY_LOW, NULL);

  static port_pin_s led0 = {.port = 0, .pin = 18}; // Change to your LED pin
  static port_pin_s led1 = {.port = 0, .pin = 24}; // Change to your LED pin

  xTaskCreate(led_task, "LED0", 2048 / sizeof(void *), &led0, PRIORITY_LOW, NULL);
  xTaskCreate(led_task, "LED1", 2048 / sizeof(void *), &led1, PRIORITY_LOW, NULL);

  // Start the scheduler
  vTaskStartScheduler();

  // If all is well, the scheduler will now be running, and the following line will never be reached.
  // If the following line does execute, then there was insufficient FreeRTOS heap memory available for the idle and/or
  // timer tasks to be created.

  // xTaskCreate(led_task, “led1”, 2048 / sizeof(void *), NULL, 1, NULL);

  // If you have the ESP32 wifi module soldered on the board, you can try uncommenting this code
  // See esp32/README.md for more details
  // uart3_init();                                                                     // Also include:  uart3_init.h
  // xTaskCreate(esp32_tcp_hello_world_task, "uart3", 1000, NULL, PRIORITY_LOW, NULL); // Include esp32_task.h

  puts("Starting RTOS");
  vTaskStartScheduler(); // This function never returns unless RTOS scheduler runs out of memory and fails
  for (;;)
    ;
  return 0; // Should not reach here
}

static void create_blinky_tasks(void) {
  /**
   * Use '#if (1)' if you wish to observe how two tasks can blink LEDs
   * Use '#if (0)' if you wish to use the 'periodic_scheduler.h' that will spawn 4 periodic tasks, one for each LED
   */
#if (1)
  // These variables should not go out of scope because the 'blink_task' will reference this memory
  static gpio_s led0, led1;

  // If you wish to avoid malloc, use xTaskCreateStatic() in place of xTaskCreate()
  static StackType_t led0_task_stack[512 / sizeof(StackType_t)];
  static StackType_t led1_task_stack[512 / sizeof(StackType_t)];
  static StaticTask_t led0_task_struct;
  static StaticTask_t led1_task_struct;

  led0 = board_io__get_led0();
  led1 = board_io__get_led1();

  xTaskCreateStatic(blink_task, "led0", ARRAY_SIZE(led0_task_stack), (void *)&led0, PRIORITY_LOW, led0_task_stack,
                    &led0_task_struct);
  xTaskCreateStatic(blink_task, "led1", ARRAY_SIZE(led1_task_stack), (void *)&led1, PRIORITY_LOW, led1_task_stack,
                    &led1_task_struct);
#else
  periodic_scheduler__initialize();
  UNUSED(blink_task);
#endif
}

static void create_uart_task(void) {
  // It is advised to either run the uart_task, or the SJ2 command-line (CLI), but not both
  // Change '#if (0)' to '#if (1)' and vice versa to try it out
#if (0)
  // printf() takes more stack space, size this tasks' stack higher
  xTaskCreate(uart_task, "uart", (512U * 8) / sizeof(void *), NULL, PRIORITY_LOW, NULL);
#else
  sj2_cli__init();
  UNUSED(uart_task); // uart_task is un-used in if we are doing cli init()
#endif
}

static void blink_task(void *params) {
  const gpio_s led = *((gpio_s *)params); // Parameter was input while calling xTaskCreate()

  // Warning: This task starts with very minimal stack, so do not use printf() API here to avoid stack overflow
  while (true) {
    gpio__toggle(led);
    vTaskDelay(500);
  }
}

// This sends periodic messages over printf() which uses system_calls.c to send them to UART0
static void uart_task(void *params) {
  TickType_t previous_tick = 0;
  TickType_t ticks = 0;

  while (true) {
    // This loop will repeat at precise task delay, even if the logic below takes variable amount of ticks
    vTaskDelayUntil(&previous_tick, 2000);

    /* Calls to fprintf(stderr, ...) uses polled UART driver, so this entire output will be fully
     * sent out before this function returns. See system_calls.c for actual implementation.
     *
     * Use this style print for:
     *  - Interrupts because you cannot use printf() inside an ISR
     *    This is because regular printf() leads down to xQueueSend() that might block
     *    but you cannot block inside an ISR hence the system might crash
     *  - During debugging in case system crashes before all output of printf() is sent
     */
    ticks = xTaskGetTickCount();
    fprintf(stderr, "%u: This is a polled version of printf used for debugging ... finished in", (unsigned)ticks);
    fprintf(stderr, " %lu ticks\n", (xTaskGetTickCount() - ticks));

    /* This deposits data to an outgoing queue and doesn't block the CPU
     * Data will be sent later, but this function would return earlier
     */
    ticks = xTaskGetTickCount();
    printf("This is a more efficient printf ... finished in");
    printf(" %lu ticks\n\n", (xTaskGetTickCount() - ticks));
  }
}
