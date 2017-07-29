#include "stm32f1xx_hal.h"
#include "stdbool.h"
#include "main.h"
#include "portsMgmnt.h"
#include "timeMgmnt.h"
#include "intrinsics.h"

extern machineParameters wa;                           // состояние автомата
extern counters cnt;

volatile bool userButton = false,       servUpButton = false,           servDownButton = false,
              servLeftButton = false,   servRightButton = false;

timeStr buttonsDisabledTime = {0}, sensorsDisabledTime = {0};

void clrUserButton() {
  userButton = false;
}
void clrServUpButton() {
  servUpButton = false;
}
void clrServDownButton() {
  servDownButton = false;
}
void clrServLeftButton() {
  servLeftButton = false;
}
void clrServRightButton() {
  servRightButton = false;
}

bool isUserButtonPressed() {
  return userButton;
}
bool isAdminUpButtonPressed() {
  return servUpButton;
}
bool isAdminDownButtonPressed() {
  return servDownButton;
}
bool isAdminLeftButtonPressed() {
  return servLeftButton;
}
bool isAdminRightButtonPressed() {
  return servRightButton;
}

void disableSensorsForTime (void) {
  writeTime(&sensorsDisabledTime);
}

void disableButtonsForTime (void) {
  writeTime(&sensorsDisabledTime);
}

  
void checkTumperDoor(){
  if (READ_DOOR_TUMPER()) wa.tumperDoor = DETECTED;
  else wa.tumperDoor = OK;
}

void checkMagistralPressure(){
  if (READ_MAG_PRESURE()) wa.magistralPressure = HI_PRESSURE;
  else wa.magistralPressure = NO_PRESSURE;
}

void checkTumperMoney(){
  if (READ_MONEY_TUMPER()) wa.tumperMoney = OK;
  else wa.tumperMoney = DETECTED;
}

void checkFreeMode(){
  if (READ_FREE_INPUT()) wa.free = NOT_FREE;
  else wa.free = FREE_MODE;
}



// 10 литровый расходомер на выходе у потребителя
void checkOut10Counter(){
  static uint8_t inData[10];
  static uint8_t p = 0, swchr = 0;
  
  static uint8_t temp = 0;
  if (temp++ < 5) return;
  temp = 0;
  
  inData[p++] = READ_10L_OUT();
  if (p > 9) p = 0;
  
  static int8_t lastInState = -100;
  if ((lastInState == -100 || swchr == 1) && !(inData[0]+inData[1]+inData[2]+inData[3]+inData[4]+inData[5]+inData[6]+inData[7]+inData[8]+inData[9])) {
    swchr = 0;
    if (lastInState == -101) cnt.out10Counter++;
    if (lastInState == -100) lastInState = -101;
  }
  if ((lastInState == -100 || swchr == 0) && (inData[0]+inData[1]+inData[2]+inData[3]+inData[4]+inData[5]+inData[6]+inData[7]+inData[8]+inData[9]) == 10) {
    swchr = 1;
    if (lastInState == -110) cnt.out10Counter++;
    if (lastInState == -100) lastInState = -110;
  }
}

// 10 литровый расходомер на входе системы
void checkInput10Counter(){
  static uint8_t inData[10];
  static uint8_t p = 0, swchr = 0;
  
  static uint8_t temp = 0;
  if (temp++ < 5) return;
  temp = 0;
  
  inData[p++] = READ_10L_IN();
  if (p > 9) p = 0;
  
  static int8_t lastInState = -100;
  if ((lastInState == -100 || swchr == 1) && !(inData[0]+inData[1]+inData[2]+inData[3]+inData[4]+inData[5]+inData[6]+inData[7]+inData[8]+inData[9])) {
    swchr = 0;
    if (lastInState == -101) cnt.input10Counter++;
    if (lastInState == -100) lastInState = -101;
  }
  if ((lastInState == -100 || swchr == 0) && (inData[0]+inData[1]+inData[2]+inData[3]+inData[4]+inData[5]+inData[6]+inData[7]+inData[8]+inData[9]) == 10) {
    swchr = 1;
    if (lastInState == -110) cnt.input10Counter++;
    if (lastInState == -100) lastInState = -110;
  }
}


static uint32_t containerCounter = 0;
// Простой расходомер на входе в контейнер
void countContainerHandler(){
  containerCounter++;
  cnt.milLitContIn = (uint32_t)((double)containerCounter * (10000.0)/(double)VAL_FOR_10_LITERS); 
}

// Простой расходомер, датчик отсутствия тары
void pauseOutHandler(){
  static timeStr lastTime = {0};
  static uint8_t fastPulseLoseOut = 0;
  static uint32_t outLoseCounter = 0; 
  
  outLoseCounter++;
  
  if (getTimeDiff(lastTime) < 500) fastPulseLoseOut++;
  else fastPulseLoseOut = 0;
  
  if (fastPulseLoseOut > FAST_PULSES_NUM_TRESHOLD) {
    wa.waterMissDetected = true;
  }
  writeTime(&lastTime);
}

// Простой расходомер, датчик перелива из емкости
void countLoseHandler() {
  static timeStr lastTime = {0};
  static uint8_t fastPulseLose = 0;
  static uint32_t loseCounter = 0;
  
  loseCounter++; 
  cnt.milLitloseCounter = (uint32_t)((double)loseCounter * (10000.0)/(double)VAL_FOR_10_LITERS); 

  if (getTimeDiff(lastTime) < 500) fastPulseLose++;
  else fastPulseLose = 0;
  writeTime(&lastTime);
  
  if (fastPulseLose > FAST_PULSES_NUM_TRESHOLD) {
    wa.container = FULL;                                                        // we know container is full
    wa.currentContainerVolume = wa.container;
    while (cnt.milLitContIn < cnt.milLitWentOut + wa.container * 1000) {        // then milLitContIn have to be bigger then milLitWentOut on container volume
      containerCounter++; 
      cnt.milLitContIn = (uint32_t)((double)containerCounter * (10000.0)/(double)VAL_FOR_10_LITERS); 
    }
  }
}

// Простой расходомер на потребителя 
void countOutHandler(){
  static uint32_t outCounter = 0; 
  
  outCounter++;
  cnt.milLitWentOut = (uint32_t)((double)outCounter * (10000.0)/(double)VAL_FOR_10_LITERS); 
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
  if (getTimeDiff(sensorsDisabledTime) > TIME_TO_DISABLE_SENSORS) {
    static uint16_t pinNum = 0;
    pinNum = GPIO_Pin;
    if (GPIO_Pin == 2) {}
    if (GPIO_Pin == 4) {}
    if (GPIO_Pin == 8) {}
    if (GPIO_Pin == 16) {}
    
    if (GPIO_Pin == 32) {                       // PE1, R12
      countContainerHandler();
    }
    if (GPIO_Pin == 1) {                        // PE0, R13
      countOutHandler();
    }
    if (GPIO_Pin == 128) {                      // PB9, R15
      countLoseHandler();
    }
    if (GPIO_Pin == 256) {                      // PB8, R19
      pauseOutHandler();
    }
    
    if (GPIO_Pin == 1024) {}
    if (GPIO_Pin == 2048) {}
    if (GPIO_Pin == 4096) {}
  }
  
  if (getTimeDiff(buttonsDisabledTime) > TIME_TO_DISABLE_BUTTONS) {  
    if (GPIO_Pin == 256) {                // R19
      userButton = true;
    }
    if (GPIO_Pin == 8192) {               // S1
      servUpButton = true;
    }
    if (GPIO_Pin == 16384) {              // S2
      servRightButton = true;
    }
    if (GPIO_Pin == 32768) {              // S3
      servDownButton = true;
    }
    if (GPIO_Pin == 64) {                 // S4
      servLeftButton = true;
    }
  }
}


