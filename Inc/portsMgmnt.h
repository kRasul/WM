#ifndef PORTS_MGMNT_H_
#define PORTS_MGMNT_H_

#include "main.h"
#include "stdbool.h"

// 1. Для управления главным насосом
// 2. Для управления клапаном магистральным
// 3. Для управления насосом выдачи
// 4. Для управления клапаном промывочным
// 5. Для управления обогревом
// 6. Для управления охлаждением

#define MAIN_PUMP_SET()         HAL_GPIO_WritePin(PWR1_GPIO_Port, PWR1_Pin, GPIO_PIN_SET)
#define MAIN_PUMP_CLR()         HAL_GPIO_WritePin(PWR1_GPIO_Port, PWR1_Pin, GPIO_PIN_RESET)
#define MAIN_VALVE_SET()        HAL_GPIO_WritePin(PWR2_GPIO_Port, PWR2_Pin, GPIO_PIN_SET)
#define MAIN_VALVE_CLR()        HAL_GPIO_WritePin(PWR2_GPIO_Port, PWR2_Pin, GPIO_PIN_RESET)
#define CONSUM_PUMP_SET()       HAL_GPIO_WritePin(PWR3_GPIO_Port, PWR3_Pin, GPIO_PIN_SET)
#define CONSUM_PUMP_CLR()       HAL_GPIO_WritePin(PWR3_GPIO_Port, PWR3_Pin, GPIO_PIN_RESET)
#define WASH_FILV_SET()         HAL_GPIO_WritePin(PWR4_GPIO_Port, PWR4_Pin, GPIO_PIN_SET)
#define WASH_FILV_CLR()         HAL_GPIO_WritePin(PWR4_GPIO_Port, PWR4_Pin, GPIO_PIN_RESET)
#define WARM_SET()              HAL_GPIO_WritePin(PWR5_GPIO_Port, PWR5_Pin, GPIO_PIN_SET)
#define WARM_CLR()              HAL_GPIO_WritePin(PWR5_GPIO_Port, PWR5_Pin, GPIO_PIN_RESET)
#define COLD_SET()              HAL_GPIO_WritePin(PWR6_GPIO_Port, PWR6_Pin, GPIO_PIN_SET)
#define COLD_CLR()              HAL_GPIO_WritePin(PWR6_GPIO_Port, PWR6_Pin, GPIO_PIN_RESET)

#define INHIBIT_C_SET()         HAL_GPIO_WritePin(GPIO7_GPIO_Port, GPIO7_Pin, GPIO_PIN_SET)
#define INHIBIT_C_CLR()         HAL_GPIO_WritePin(GPIO7_GPIO_Port, GPIO7_Pin, GPIO_PIN_RESET)
#define INHIBIT_M_SET()         HAL_GPIO_WritePin(GPIO6_GPIO_Port, GPIO6_Pin, GPIO_PIN_SET)
#define INHIBIT_M_CLR()         HAL_GPIO_WritePin(GPIO6_GPIO_Port, GPIO6_Pin, GPIO_PIN_RESET)

#define MAINP_ON()              do {wa.mainPump = WORKING; disableSensorsForTime(); MAIN_PUMP_SET();} while(0)       
#define MAINP_OFF()             do {wa.mainPump = STOPPED; disableSensorsForTime(); MAIN_PUMP_CLR();} while(0)       
#define CONSUMP_ON()            do {wa.consumerPump = WORKING; disableSensorsForTime(); CONSUM_PUMP_SET();} while(0)
#define CONSUMP_OFF()           do {wa.consumerPump = STOPPED; disableSensorsForTime(); CONSUM_PUMP_CLR();} while(0)
#define MAINV_ON()              do {wa.mainValve = OPENED; disableSensorsForTime(); MAIN_VALVE_SET();} while(0)             
#define MAINV_OFF()             do {wa.mainValve = CLOSED; disableSensorsForTime(); MAIN_VALVE_CLR();} while(0)             
#define WASH_FILV_ON()          do {wa.washFilValve = OPENED; disableSensorsForTime(); WASH_FILV_SET();} while(0)             
#define WASH_FILV_OFF()         do {wa.washFilValve = CLOSED; disableSensorsForTime(); WASH_FILV_CLR();} while(0)       
#define WARM_ON()               do {wa.warmer = TURNED_ON; disableSensorsForTime(); WARM_SET();} while(0)             
#define WARM_OFF()              do {wa.warmer = TURNED_OFF; disableSensorsForTime(); WARM_CLR();} while(0)       
#define COLD_ON()               do {wa.cooler = TURNED_ON; disableSensorsForTime(); COLD_SET();} while(0)             
#define COLD_OFF()              do {wa.cooler = TURNED_OFF; disableSensorsForTime(); COLD_CLR();} while(0)       

#define INHIBIT_EN()            do {INHIBIT_C_SET(); INHIBIT_M_SET(); wa.billAccept = ENABLE_BILL;}     while(0)            
#define INHIBIT_DIS()           do {INHIBIT_C_CLR(); INHIBIT_M_CLR(); wa.billAccept = INHIBIT_BILL;}    while(0)   

#define READ_DOOR_TUMPER()      HAL_GPIO_ReadPin(NINT_IN15_GPIO_Port, NINT_IN15_Pin)            // R1
#define READ_MONEY_TUMPER()     HAL_GPIO_ReadPin(NINT_IN16_GPIO_Port, NINT_IN16_Pin)            // R2
#define READ_MAG_PRESURE()      HAL_GPIO_ReadPin(NINT_IN17_GPIO_Port, NINT_IN17_Pin)            // R3
#define READ_FREE_INPUT()       HAL_GPIO_ReadPin(NINT_IN18_GPIO_Port, NINT_IN18_Pin)            // R4
#define READ_10L_OUT()          HAL_GPIO_ReadPin(NINT_IN19_GPIO_Port, NINT_IN19_Pin)            // R5
#define READ_10L_IN()           HAL_GPIO_ReadPin(NINT_IN20_GPIO_Port, NINT_IN20_Pin)            // R6

void checkTumperDoor();
void checkMagistralPressure();
void checkTumperMoney();
void checkFreeMode();
void checkInput10Counter();
void checkOut10Counter();

void clrUserButton();
void clrServUpButton();
void clrServDownButton();
void clrServLeftButton();
void clrServRightButton();

bool isUserButtonPressed();
bool isAdminUpButtonPressed();
bool isAdminDownButtonPressed();
bool isAdminLeftButtonPressed();
bool isAdminRightButtonPressed();

#define TIME_TO_DISABLE_SENSORS         100
#define TIME_TO_DISABLE_BUTTONS         100         
void disableSensorsForTime(void);
void disableButtonsForTime(void);


#endif