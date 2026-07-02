/**
 * @file    mcu2Pins.h
 * @brief   U31 STM32G070CBT6 pin map — aligned with docs/MCU2_G070/PINMAP.md
 * @author  2026-07-01
 */
#ifndef MCU2_PINS_H
#define MCU2_PINS_H

#include "stm32g0xx_hal.h"

/* SPI1 slave: interconnect with U13 F103 SPI1_* nets */
#define MCU2_SPI1_CS_PORT      GPIOA
#define MCU2_SPI1_CS_PIN       GPIO_PIN_4

#define MCU2_SPI1_CLK_PORT     GPIOA
#define MCU2_SPI1_CLK_PIN      GPIO_PIN_5

#define MCU2_SPI1_MISO_PORT    GPIOA
#define MCU2_SPI1_MISO_PIN     GPIO_PIN_6

#define MCU2_SPI1_MOSI_PORT    GPIOA
#define MCU2_SPI1_MOSI_PIN     GPIO_PIN_7

/* ADS1220 U27: bit-bang SPI on PB3–PB7 (PB4 is not HW SPI2_SCK) */
#define MCU2_ADS_CS_PORT       GPIOB
#define MCU2_ADS_CS_PIN        GPIO_PIN_3

#define MCU2_ADS_SCK_PORT      GPIOB
#define MCU2_ADS_SCK_PIN       GPIO_PIN_4

#define MCU2_ADS_DRDY_PORT     GPIOB
#define MCU2_ADS_DRDY_PIN      GPIO_PIN_5

#define MCU2_ADS_MISO_PORT     GPIOB
#define MCU2_ADS_MISO_PIN      GPIO_PIN_6

#define MCU2_ADS_MOSI_PORT     GPIOB
#define MCU2_ADS_MOSI_PIN      GPIO_PIN_7

/* External WDT feed / indicator LEDs */
#define MCU2_WDI_PORT          GPIOB
#define MCU2_WDI_PIN           GPIO_PIN_0

#define MCU2_COM_LED_PORT      GPIOB
#define MCU2_COM_LED_PIN       GPIO_PIN_1

#define MCU2_RUN_LED_PORT      GPIOD
#define MCU2_RUN_LED_PIN       GPIO_PIN_3

/* Reset interlock (input, optional) */
#define MCU2_R_RST_PORT        GPIOA
#define MCU2_R_RST_PIN         GPIO_PIN_2

#endif /* MCU2_PINS_H */
