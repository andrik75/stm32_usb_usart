/**
  * @file    critical_section.c
  * @author  Andriy Bratus <ambr75@gmail.com>
  * @brief   Header file for STM32 critical section implementation.
  * @date    2026
  *
  * @copyright Copyright (c) 2026 Andriy Bratus <ambr75@gmail.com>
  *            All rights reserved.
  *
  * @attention
  * SPDX-License-Identifier: GPL-3.0-or-later
  */
#ifndef INC_CRITICAL_SECTION_H_
#define INC_CRITICAL_SECTION_H_

#include "cmsis_gcc.h"

/**
 * @brief Enter critical section: save current interrupt state and disable interrupts.
 */
#define CRITICAL_SECTION_ENTER() \
    uint32_t primask_bit = __get_PRIMASK(); \
    __disable_irq()

/**
 * @brief Exit critical section: restore previous interrupt state.
 */
#define CRITICAL_SECTION_EXIT() \
    __set_PRIMASK(primask_bit)

/**
 * @brief Safe critical section wrapper macro.
 *        Executes the provided code block with interrupts disabled,
 *        then restores the previous interrupt state.
 */
#define CRITICAL_SECTION() \
    for (uint32_t primask_bit = __get_PRIMASK(), i = (__disable_irq(), 1); \
         i; \
         i = 0, __set_PRIMASK(primask_bit))

#endif /* INC_CRITICAL_SECTION_H_ */
