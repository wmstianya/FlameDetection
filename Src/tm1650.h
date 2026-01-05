#ifndef __TM1650_H
#define __TM1650_H

#include "main.h"

void tm1650_gpio_init(void);
void disp(uint8_t addr,uint8_t value);

void Send_To_TM1650(void);
void dis_value(uint16_t data);
void dis_text_temp(void);
#endif

