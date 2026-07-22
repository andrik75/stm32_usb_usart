/**
  * @file    config.c
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Configuration file for all the custom libraries.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */
#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

/* -------------------------------------------------------------------------
 * Stringification Macros (Convert a macro value into a string literal)
 * Two levels are required to force the preprocessor to expand the macro argument 
 * before converting it to a string.
 * ------------------------------------------------------------------------- */
#define STR_INDIR(x) #x
#define STR(x)       STR_INDIR(x)

/* -------------------------------------------------------------------------
 * Token Concatenation Macros (Join two tokens together)
 * Two levels are required to ensure macros passed as arguments are fully 
 * expanded prior to token pasting (##).
 * ------------------------------------------------------------------------- */
#define CONCAT_INDIR(a, b) a ## b
#define CONCAT(a, b)       CONCAT_INDIR(a, b)

/* -------------------------------------------------------------------------
 * Hardware Configuration
 * ------------------------------------------------------------------------- */
#define MCU_FAMILY stm32f1xx

/* Concatenates target family name with HAL header suffix (produces stm32f1xx_hal.h token) */
#define MCU_HAL_HEADER CONCAT(MCU_FAMILY, _hal.h)

/* Includes the calculated header file wrapped in string quotes "stm32f1xx_hal.h" */
#include STR(MCU_HAL_HEADER)

#endif /* INC_CONFIG_H_ */
