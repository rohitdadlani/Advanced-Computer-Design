#include <stdio.h>

#include "FreeRTOS.h"
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

int main(void) {
  create_blinky_tasks();
  create_uart_task();

  // If you have the ESP32 wifi module soldered on the board, you can try uncommenting this code
  // See esp32/README.md for more details
  // uart3_init();                                                                     // Also include:  uart3_init.h
  // xTaskCreate(esp32_tcp_hello_world_task, "uart3", 1000, NULL, PRIORITY_LOW, NULL); // Include esp32_task.h

  puts("Starting RTOS");
  vTaskStartScheduler(); // This function never returns unless RTOS scheduler runs out of memory and fails

  return 0;
}

// Assuming P1.10 for CS
#define ADESTO_FLASH_CS_PORT 1
#define ADESTO_FLASH_CS_PIN 10

void adesto_cs_init(void) {
  LPC_GPIO1->DIR |= (1 << ADESTO_FLASH_CS_PIN); // Set as output
  adesto_ds();                                  // Deselect the flash by default
}

void adesto_cs(void) {
  LPC_GPIO1->CLR = (1 << ADESTO_FLASH_CS_PIN); // Activate CS (active low)
}

void adesto_ds(void) {
  LPC_GPIO1->SET = (1 << ADESTO_FLASH_CS_PIN); // Deactivate CS (set high)
}

adesto_flash_id_s adesto_read_signature(void) {
  adesto_flash_id_s data = {0};

  adesto_cs();

  ssp2__exchange_byte(0x9F);                        // Send "Read ID" command (0x9F)
  data.manufacturer_id = ssp2__exchange_byte(0xFF); // Dummy byte for clocking out response
  data.device_id_1 = ssp2__exchange_byte(0xFF);
  data.device_id_2 = ssp2__exchange_byte(0xFF);
  data.extended_device_id = ssp2__exchange_byte(0xFF);

  adesto_ds();

  return data;
}

void configure_ssp2_pins(void) {
  // Example pin configuration
  LPC_IOCON->P1_0 = 0x4; // SCK2 function
  LPC_IOCON->P1_1 = 0x4; // MOSI2 function
  LPC_IOCON->P1_4 = 0x4; // MISO2 function
}

void spi_task(void *p) {
  const uint32_t spi_clock_mhz = 24;

  configure_ssp2_pins();     // Configure SSP2 pin functions
  adesto_cs_init();          // Initialize CS GPIO
  ssp2__init(spi_clock_mhz); // Initialize SSP2

  while (1) {
    adesto_flash_id_s id = adesto_read_signature();

    // Example printf - adjust based on your printf function
    printf("Manufacturer ID: 0x%X, Device ID: 0x%X 0x%X, Extended Device ID: 0x%X\n", id.manufacturer_id,
           id.device_id_1, id.device_id_2, id.extended_device_id);

    vTaskDelay(500);
  }
}

SemaphoreHandle_t spi_mutex;

void spi_init() {
  spi_mutex = xSemaphoreCreateMutex();
  ssp2__init(24); // Initialize SPI with desired clock rate
  // Don't forget to configure SSP2 pins and Adesto flash CS GPIO as shown previously
}

void adesto_read_signature_with_mutex(void) {
  if (xSemaphoreTake(spi_mutex, portMAX_DELAY)) {
    // Your existing adesto_read_signature code here

    xSemaphoreGive(spi_mutex);
  }
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
