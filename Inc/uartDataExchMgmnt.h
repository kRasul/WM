#ifndef UART_RPI_H_
#define UART_RPI_H_

#include <stdbool.h>

#define RPI_BUFFER_SIZE         256

void uartDataExchMgmnt(void);

void initUART(void);

#endif