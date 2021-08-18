#ifndef UART_CTRL_H
#define UART_CTRL_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#define RX_BUF_SIZE   256
#define TXD_PIN (GPIO_NUM_25)
#define RXD_PIN (GPIO_NUM_26)

void uartInit(void);

int uartSend(const char* data);

#endif