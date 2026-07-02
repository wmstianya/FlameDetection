/**
 * @file    stm32g0xx_it.h
 * @brief   MCU2 interrupt handlers
 */
#ifndef STM32G0XX_IT_H
#define STM32G0XX_IT_H

void NMI_Handler(void);
void HardFault_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void SPI1_IRQHandler(void);
void EXTI4_15_IRQHandler(void);

#endif /* STM32G0XX_IT_H */
