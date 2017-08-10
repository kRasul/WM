#include "stm32f1xx_hal.h"
#include "stdbool.h"
#include "main.h"
#include "portsMgmnt.h"
#include "timeMgmnt.h"
#include "intrinsics.h"

extern machineParameters wa;                           // состояние автомата
extern counters cnt;
extern uint32_t valFor10LitCalibr;

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
  writeTime(&buttonsDisabledTime);
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

static uint32_t containerCounter = 0;
// Простой расходомер на входе в контейнер
void countContainerHandler(){
  containerCounter++;
  cnt.milLitContIn = (uint32_t)((double)containerCounter * (10000.0)/(double)valFor10LitCalibr); 
}

void setupDefaultLitersVolume(uint16_t volume) {
  containerCounter = valFor10LitCalibr/10 * volume;
  cnt.milLitContIn = (uint32_t)((double)containerCounter * (10000.0)/(double)valFor10LitCalibr); 
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
extern uint8_t maxContainerVolume;                // объем контейнера с водой
void countLoseHandler() {
  static timeStr lastTime = {0};
  static uint8_t fastPulseLose = 0;
  static uint32_t loseCounter = 0;
  
  loseCounter++; 
  cnt.milLitloseCounter = (uint32_t)((double)loseCounter * (10000.0)/(double)valFor10LitCalibr); 

  if (getTimeDiff(lastTime) < 500) fastPulseLose++;
  else fastPulseLose = 0;
  writeTime(&lastTime);
  
  if (fastPulseLose > FAST_PULSES_NUM_TRESHOLD) {
    wa.container = FULL;                                                        // we know container is full
/*    wa.currentContainerVolume = maxContainerVolume;    
    containerCounter = valFor10LitCalibr/10 * volume;
    while (cnt.milLitContIn < cnt.milLitWentOut + wa.container * 1000) {        // then milLitContIn have to be bigger then milLitWentOut on container volume
      containerCounter++; 
      cnt.milLitContIn = (uint32_t)((double)containerCounter * (10000.0)/(double)valFor10LitCalibr); 
    }
*/    
    containerCounter += valFor10LitCalibr/10 * 
      (maxContainerVolume - ((cnt.milLitContIn - cnt.milLitWentOut - cnt.milLitloseCounter) / 1000));
    cnt.milLitContIn = (uint32_t)((double)containerCounter * (10000.0)/(double)valFor10LitCalibr); 
    wa.currentContainerVolume = (cnt.milLitContIn - cnt.milLitWentOut - cnt.milLitloseCounter) / 1000;
  }
}

// Простой расходомер на потребителя 
static uint32_t outCounter = 0; 
void countOutHandler(){
  
  outCounter++;
  cnt.milLitWentOut = (uint32_t)((double)outCounter * (10000.0)/(double)valFor10LitCalibr); 
}


// 10 литровый расходомер на выходе у потребителя
void checkOut10Counter(){
  static uint8_t inDataOUT[10];
  static uint8_t p = 0, swchr = 0;
  
  static uint8_t temp = 0;
  if (temp++ < 5) return;
  temp = 0;
  
  inDataOUT[p++] = READ_10L_OUT();
  if (p > 9) p = 0;
  
  static uint32_t out101 = 0, out102 = 0;
  
  static int8_t lastInState = -100;
  if ((lastInState == -100 || swchr == 1) && !(inDataOUT[0]+inDataOUT[1]+inDataOUT[2]+inDataOUT[3]+inDataOUT[4]+inDataOUT[5]+inDataOUT[6]+inDataOUT[7]+inDataOUT[8]+inDataOUT[9])) {
    swchr = 0;
    if (lastInState == -101) {
      cnt.out10Counter++;
      out101 = outCounter;
    }
    if (lastInState == -100) lastInState = -101;
  }
  if ((lastInState == -100 || swchr == 0) && (inDataOUT[0]+inDataOUT[1]+inDataOUT[2]+inDataOUT[3]+inDataOUT[4]+inDataOUT[5]+inDataOUT[6]+inDataOUT[7]+inDataOUT[8]+inDataOUT[9]) == 10) {
    swchr = 1;
    if (lastInState == -110) {
      cnt.out10Counter++;
      out102 = outCounter;
    }
    if (lastInState == -100) lastInState = -110;
  }
}

// 10 литровый расходомер на входе системы
void checkInput10Counter(){
  static uint8_t inDataIN[10];
  static uint8_t p = 0, swchr = 0;
  
  static uint8_t temp = 0;
  if (temp++ < 5) return;
  temp = 0;
  
  inDataIN[p++] = READ_10L_IN();
  if (p > 9) p = 0;
  
  static uint32_t in101 = 0, in102 = 0;
  
  static int8_t lastInState = -100;
  if ((lastInState == -100 || swchr == 1) && !(inDataIN[0]+inDataIN[1]+inDataIN[2]+inDataIN[3]+inDataIN[4]+inDataIN[5]+inDataIN[6]+inDataIN[7]+inDataIN[8]+inDataIN[9])) {
    swchr = 0;
    if (lastInState == -101) {
      cnt.input10Counter++;
      in101 = containerCounter;
    }
    if (lastInState == -100) lastInState = -101;
  }
  if ((lastInState == -100 || swchr == 0) && (inDataIN[0]+inDataIN[1]+inDataIN[2]+inDataIN[3]+inDataIN[4]+inDataIN[5]+inDataIN[6]+inDataIN[7]+inDataIN[8]+inDataIN[9]) == 10) {
    swchr = 1;
    if (lastInState == -110) {
      cnt.input10Counter++;
      in102 = containerCounter;
    }
    if (lastInState == -100) lastInState = -110;
  }
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
  static uint16_t lastPinNum = 0;
  static timeStr timePulseDetected = {0};
  
  static uint16_t c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11;
  
  //if (GPIO_Pin == lastPinNum && getTimeDiff(timePulseDetected) < 2) return;  
    
  if (getTimeDiff(sensorsDisabledTime) > TIME_TO_DISABLE_SENSORS) {
    
    if (GPIO_Pin == 256) {
                                                // 17
      c1++;
    }                  
    if (GPIO_Pin == 4) {
      countOutHandler();                        // 18
      c2++;                                     
    }                   
    if (GPIO_Pin == 8) {
      countLoseHandler();                       // 19
      c3++;
    }                  
    if (GPIO_Pin == 16) {
      pauseOutHandler();                        // 20
      c4++;
    }                 
    
    if (GPIO_Pin == 32) {                       
      countContainerHandler();                  // 21
      c5++;
    }
    
    if (GPIO_Pin == 1) {                        // 16 || 17, PE0, R13
      c6++;
    }
    if (GPIO_Pin == 128) {                      // PB9, R15
      c7++;
    }
    if (GPIO_Pin == 512) {                      // PB8, R19
      c8++;
    }
    
    if (GPIO_Pin == 1024) {c9++;}
    if (GPIO_Pin == 2048) {c10++;}
    if (GPIO_Pin == 4096) {c11++;}
  }
  
  if (getTimeDiff(buttonsDisabledTime) > TIME_TO_DISABLE_BUTTONS) {  
    if (GPIO_Pin == 2) {                        // R11
      static timeStr lastTimeButton = {0};
      if (getTimeDiff(lastTimeButton) > 500) {
        writeTime(&lastTimeButton);
        userButton = true; 
        static uint8_t usrBtCnt = 0;
        usrBtCnt++;
      }      
    }
    if (GPIO_Pin == 8192) {                     // S1
      static timeStr lastTimeButton = {0};
      if (getTimeDiff(lastTimeButton) > 200) {
        writeTime(&lastTimeButton);
        servUpButton = true; 
      }
    }
    if (GPIO_Pin == 16384) {                    // S2
      static timeStr lastTimeButton = {0};
      if (getTimeDiff(lastTimeButton) > 200) {
        writeTime(&lastTimeButton);
        servRightButton = true; 
      }
    }
    if (GPIO_Pin == 32768) {                    // S3
      static timeStr lastTimeButton = {0};
      if (getTimeDiff(lastTimeButton) > 200) {
        writeTime(&lastTimeButton);
        servDownButton = true; 
      }
    }
    if (GPIO_Pin == 64) {                       // S4
      static timeStr lastTimeButton = {0};
      if (getTimeDiff(lastTimeButton) > 200) {
        writeTime(&lastTimeButton);
        servLeftButton = true; 
      }
    }
  }
  lastPinNum = GPIO_Pin;
}


