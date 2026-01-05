#include "tm1650.h"
#include "main.h"



uint8_t  CODE[10]={0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};



void tm1650_gpio_init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);

  /*Configure GPIO pins : PD1 PD2 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	 HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);
	 HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_SET);
	
	 /*Configure GPIO pins : PA10 PA11 */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10|GPIO_PIN_11, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);  //LED OFF
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);//Relays  = 0
	
	
}
void clk_set(uint8_t v)
{

		HAL_GPIO_WritePin(GPIOD,GPIO_PIN_1,(GPIO_PinState)v);
}
void sda_set(uint8_t v)
{

		HAL_GPIO_WritePin(GPIOD,GPIO_PIN_2,(GPIO_PinState)v);
}

void delay_us(uint16_t delay)
{
	if(delay==0)
		return;
	uint32_t delay_t = delay*72;
	while(delay_t)
	{
		delay_t--;
	}
}
void iic_start(void)
{
	clk_set(1);
	delay_us(1);
	sda_set(1);
	delay_us(1);
	
	sda_set(0);
	delay_us(1);
	clk_set(0);
	delay_us(1);
}

void iic_stop(void)
{
	sda_set(0);
	delay_us(1);
	clk_set(1);
	delay_us(1);
	sda_set(1);
	delay_us(1);
}
uint8_t  iic_ack(void)
{
	uint8_t time_out=0;
	clk_set(0);
	delay_us(1);
	clk_set(1);
	sda_set(1);
	while(HAL_GPIO_ReadPin(GPIOD,GPIO_PIN_2))
	{
		time_out++;
		delay_us(1);
		if(time_out>100)
			return 0;
	}
	clk_set(0);
	sda_set(0);
	return 1;
}
void iic_send_byte(uint8_t data)
{
	uint8_t i=0;
	for(i=0;i<8;i++)
	{
		clk_set(0);
		if(data&0x80)
			sda_set(1);
		else
			sda_set(0);
		delay_us(1);
		clk_set(1);
		data=data<<1;
	}
}
void disp(uint8_t addr,uint8_t value)
{
	iic_start();
	iic_send_byte(0x48);
	if(iic_ack()==0)
		iic_stop();
	iic_send_byte(0x71);
	if(iic_ack()==0)
		iic_stop();
	iic_stop();
	
	
	iic_start();
	iic_send_byte(addr);
	if(iic_ack()==0)
		iic_stop();
	iic_send_byte(value);
	if(iic_ack()==0)
		iic_stop();
	iic_stop();
}

void Send_To_TM1650(void)
{
	disp(0x68,CODE[0]);
	disp(0x6A,CODE[1]);
	disp(0x6C,CODE[2]);
	disp(0x6E,CODE[3]);	
}

void dis_value(uint16_t data)
{
	uint8_t h1,h2,h3,h4;
	h1=data/1000;
	h2 =data%1000/100;
	h3 =data%100/10;
	h4 =data%10;
	if(h1)
	{
		disp(0x68,CODE[h1]);
	}
	else
		disp(0x68,0);
	if(h2)
	{
		disp(0x6A,CODE[h2]);
	}
	else{
		if(h1)
		{
			disp(0x6A,CODE[h2]);
		}else{
			disp(0x6A,0);
		}
	}
	if(h3)
	{
		disp(0x6C,CODE[h3]);
	}
	else{
		if((h1)||(h2))
		{
			disp(0x6C,CODE[h3]);
		}else{
			disp(0x6C,0);
		}
	}
	disp(0x6E,CODE[h4]);	
}

/**
  * @brief  Display "tEMP" text on 4-digit LED
  * @param  None
  * @retval None
  * @note   Uses 7-segment encoding to show letters
  */
void dis_text_temp(void)
{
	/* 7-segment codes for letters: t E m P */
	uint8_t CODE_t = 0x78;  /* 't': segments D+E+F+G */
	uint8_t CODE_E = 0x79;  /* 'E': segments A+D+E+F+G */
	uint8_t CODE_m = 0x37;  /* 'm': segments C+E+G (approximation) */
	uint8_t CODE_P = 0x73;  /* 'P': segments A+B+E+F+G */
	
	disp(0x68, CODE_t);  /* Position 1: 't' */
	disp(0x6A, CODE_E);  /* Position 2: 'E' */
	disp(0x6C, CODE_m);  /* Position 3: 'm' */
	disp(0x6E, CODE_P);  /* Position 4: 'P' */
}


