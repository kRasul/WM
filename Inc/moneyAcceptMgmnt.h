#ifndef MONEY_ACCEPT_MGMNT_H_
#define MONEY_ACCEPT_MGMNT_H_

void checkCoinMoney(void);
void checkMoney(void);

#define COIN_PORT       NINT_IN13_GPIO_Port
#define COIN_PIN        NINT_IN13_Pin

#define MONEY_PORT      NINT_IN14_GPIO_Port
#define MONEY_PIN       NINT_IN14_Pin

#endif