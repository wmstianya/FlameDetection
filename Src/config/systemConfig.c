/**
  ******************************************************************************
  * @file           : systemConfig.c
  * @brief          : System configuration initialization
  * @author         : Senior Embedded Engineer
  * @date           : 2025-11-15
  * @version        : V1.0.4
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "systemConfig.h"

/* ============================================================================
   Configuration Initialization Functions
   ============================================================================ */

/**
  * @brief  Initialize system configuration with default values
  * @param  config: pointer to configuration structure
  * @retval None
  */
void systemConfigLoadDefaults(SystemConfig* config)
{
    /* Flame detection parameters */
    config->flameThresholdMv = FLAME_THRESHOLD_MV;
    config->startupDelayMs = STARTUP_DELAY_MS;
    config->flameOffDelayMs = FLAME_OFF_DELAY_MS;
    
    /* ADC parameters */
    config->adcBufferSize = ADC_BUFFER_SIZE;
    config->vrefintSampleCount = VREFINT_SAMPLE_COUNT;
    
    /* Temperature compensation */
    config->tempRefC = TEMP_COMP_REF_C;
    config->tempCoeffMvPerC = PA1_TEMP_COEFF_MV_PER_C;
    config->tempCompEnable = TEMP_COMP_ENABLE;
    config->autoKEnable = AUTO_K_ENABLE;
    
    /* Display parameters */
    config->tempDisplayPeriodMs = TEMP_DISPLAY_PERIOD_MS;
    config->tempDisplayDurationMs = TEMP_DISPLAY_DURATION_MS;
    config->tempDisplayEnable = TEMP_DISPLAY_ENABLE;
    
    /* Debug mode */
    config->debugMode = VREFINT_DEBUG_MODE || TEMP_DEBUG_MODE;
}

/**
  * @brief  Initialize system configuration
  * @param  config: pointer to configuration structure
  * @retval None
  */
void systemConfigInit(SystemConfig* config)
{
    systemConfigLoadDefaults(config);
    
    /* Future: Load from EEPROM/Flash if available */
}

/************************ END OF FILE *****************************/

