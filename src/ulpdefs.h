/*
 * ulpfuncs.h
 *
 * Copyright 2021 Victor Chew
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <driver/adc.h>
#include <driver/rtc_io.h>
#include <soc/adc_channel.h>
#include <soc/soc.h>
#include <soc/rtc.h>
#include <soc/rtc_wdt.h>
#include <soc/rtc_cntl_reg.h>
#include <esp32/ulp.h>
#include <esp_adc_cal.h>

// Workaround to enable loading of ULP code that is > 128 words
esp_err_t patched_ulp_process_macros_and_load(uint32_t load_addr, const ulp_insn_t* program, size_t* psize);

// Constants
#include "clock30cm.h"
#define ULP_PROG_START          200                               // ULP code starts here; region before this reserved for variables and stack
#define ULP_CALL_PER_SEC        8                                 // Number of times ULP is called per sec
#define TICKPIN1_GPIO           GPIO_NUM_25 
#define TICKPIN2_GPIO           GPIO_NUM_27 
#define RESETBTN_PIN_GPIO       GPIO_NUM_4
#define TICKPIN1                RTCIO_GPIO25_CHANNEL 
#define TICKPIN2                RTCIO_GPIO27_CHANNEL
#define RESETBTN_PIN            RTCIO_GPIO4_CHANNEL
#define VDD_CHANNEL             ADC1_GPIO33_CHANNEL               // Maps to GPIO33
#define SUPPLY_VHIGH            (SUPPLY_VLOW + 200)               // By default 0.2v higher than SUPPLY_VLOW
#define TOLERANCE_SS            30                                // If diff(clock time, network time) < tolerance (secs), skip ffwd/reverse (1-59)
#define MAX_PULSE_MS            60                                // Each pulse should take exactly this long to execute (msecs)
#define NORM_TICK_FILLER_MS     (MAX_PULSE_MS-NORM_TICK_MS)       // Length to filler in msecs for rest of forward tick cycle
#define FWD_TICK_FILLER_MS      (MAX_PULSE_MS-FWD_TICK_MS)        // Length to filler in msecs for rest of forward tick cycle
#define REV_TICKA_FILLER_MS     (MAX_PULSE_MS-REV_TICKA_T1_MS-REV_TICKA_T2_MS-REV_TICKA_T3_MS) // Length to filler in msecs for rest of reverse tick cycle
#define REV_TICKB_FILLER_MS     (MAX_PULSE_MS-REV_TICKB_T1_MS-REV_TICKB_T2_MS-REV_TICKB_T3_MS) // Length to filler in msecs for rest of reverse tick cycle

// Branch labels
enum {
  LBL_STRESS_TEST, LBL_CHECK_VDD, LBL_CHECK_RESETBTN, LBL_CHECK_PAUSE_CLOCK, LBL_DO_TICK_ACTION, LBL_COMPUTE_TICK_ACTION, LBL_CHECK_TUNE_ULP_TIMER, 
  LBL_FN_NORM_TICK, LBL_FN_FWD_TICK, LBL_FN_REV_TICKA, LBL_FN_REV_TICKB, LBL_FN_INC_CLOCK, LBL_FN_DEC_CLOCK, LBL_FN_CALC_TIME_DIFF, LBL_FN_IS_DIFF_LESS_THAN,
  LBL_COMMON_RESTART_CLOCK, LBL_COMMON_HALT, LBL_COMMON_WAKE,
  LBL_NEXT = 100, LBL_MARKER = 2000, LBL_MARKER_NEXT = 1000,
};

// Named indices into RTC_SLOW_MEM
enum {
  VAR_NET_HH,             // Net time hour
  VAR_NET_MM,             // Net time minute
  VAR_NET_SS,             // Net time second
  VAR_PAUSE_CLOCK,        // ULP code will check this flag at the start and halt if set to 1
  VAR_PREV_TACTION,       // Previous tick action
  VAR_TICK_ACTION,        // Action to take when ULP is next called
  VAR_TICK_DELAY,         // Number to ULP calls to delay before tick resumes (set to >0 when ticking direction changes)
  VAR_TICKPIN,            // Current tickpin: 0 or 1
  VAR_TUNE_LEVEL,         // 0 - 4; index into TUNE_INTERVALS to decide how often to tune ULP_TIMER
  VAR_SLEEP_COUNT,        // Sleep counter in seconds; main CPU is woken up with WAKE_TUNE_ULP_TIMER when this reaches SLEEP_INTERVAL
  VAR_SLEEP_INTERVAL,     // How long to sleep (secs) before waking main CPU with WAKE_TUNE_ULP_TIMER
  VAR_ULP_CALL_COUNT,     // Incremented every time ULP is called. When this reaches ULP_CALL_PER_SEC, it implies a second has passed
  VAR_ULP_TIMERL,         // Low word of ULP timer
  VAR_ULP_TIMERH,         // High word of ULP timer
  VAR_CLK_HH,             // Clock hour
  VAR_CLK_MM,             // Clock minute
  VAR_CLK_SS,             // Clock second
  VAR_ADC_VDD,            // ADC value of supply voltage
  VAR_ADC_VDDL,           // ADC value of SUPPLY_VLOW
  VAR_ADC_VDDH,           // ADC value of SUPPLY_VHIGH
  VAR_WAKE_REASON,        // Reason for waking up main CPU
  VAR_DIFF_HH,            // Used for storing difference between clock and net time computed by LBL_FN_TIME_DIFF
  VAR_DIFF_MM,
  VAR_DIFF_SS,
  VAR_DIFF_PACKED,        // Stores result from LBL_FN_TIME_DIFF in 16-bit HHHHMMMMMMSSSSSS packed format 
  VAR_UPDATE_PENDING,     // If >0, decrement every sec. When decremented to 0, wake main CPU with WAKE_UPDATE_NETTIME
  VAR_DEBUG,
  VAR_STACK_PTR,          // Pointer to stack that begins at VAR_LAST
  VAR_STACK_REGION,       // Start of stack
};

// Wake reasons
enum {
  WAKE_NONE,
  WAKE_RESET_BUTTON,
  WAKE_UPDATE_NETTIME,
  WAKE_TUNE_ULP_TIMER,
  WAKE_DEBUG,
};

// Tick actions
enum {
  TICK_NONE,
  TICK_NORMAL,
  TICK_FWD,
  TICK_REV,
};

/**
 * Alias for branching if R0 > 0
 */
#define X_BGZ(label) \
    M_BGE(label, 1)

/**
 * Alias for branching if R0 == 0
 */
#define X_BZ(label) \
    M_BL(label, 1)

/**
 * Branch to label if reg == value
 * Uses R3 for operation
 */
#define X_BEQ(label, reg, value) \
    I_MOVR(R3, reg), \
    I_SUBI(R3, R3, value), \
    M_BXZ(label)

/**
 * Helper function for X_BNE()
 */
#define __X_BNE(label, reg, value, marker) \
    I_MOVR(R3, reg), \
    I_SUBI(R3, R3, value), \
    M_BXZ(marker), \
    M_BX(label), \
  M_LABEL(marker)

/**
 * Branch to label if reg != value
 * Uses R3 for operation
 */
#define X_BNE(label, reg, value) \
    __X_BNE(label, reg, value,  LBL_MARKER+__LINE__)

/**
 *  Load RTCMEM[var] into register (R0 - R2).
 */
#define X_RTC_GETR(var, reg) \
    I_MOVI(R3, var), \
    I_LD(reg, R3, 0)

/**
 * Save register value (R0 - R2) into RTCMEM[var].
 */
#define X_RTC_SETR(var, reg) \
    I_MOVI(R3, var), \
    I_ST(reg, R3, 0)

/**
 * Save constant value into RTCMEM[var].
 * Uses R2 - R3 for operation
 */ 
#define X_RTC_SETI(var, value) \
    I_MOVI(R3, var), \
    I_MOVI(R2, value), \
    I_ST(R2, R3, 0)

/**
 * Set RTCMEM[dest_var] = RTCMEM[src_var]
 * Uses R2 - R3 for operation
 */
#define X_RTC_SETV(dest_var, src_var) \
    X_RTC_GETR(src_var, R2), \
    X_RTC_SETR(dest_var, R2)

/**
 * Increment RTCMEM[var] by 1.
 * Uses R3 for operation
 * R0 holds the final value of the variable.
 */
#define X_RTC_INC(var) \
    X_RTC_GETR(var, R0), \
    I_ADDI(R0, R0, 1), \
    X_RTC_SETR(var, R0)

/**
 * Decrement RTCMEM[var] by 1.
 * Uses R3 for operation
 * R0 holds the final value of the variable.
 */
#define X_RTC_DEC(var) \
    X_RTC_GETR(var, R0), \
    I_SUBI(R0, R0, 1), \
    X_RTC_SETR(var, R0)

/**
 * Branch to given label if RTCMEM[var1] < RTCMEM[var2]
 * Uses R1 - R3 for operation
 */
#define X_RTC_BLV(label, var1, var2) \
    X_RTC_GETR(var1, R1), \
    X_RTC_GETR(var2, R2), \
    I_SUBR(R1, R1, R2), \
    M_BXF(label)

/**
 * Branch to given label if RTCMEM[var1] >= RTCMEM[var2]
 * Uses R1 - R3 for operation
 */
#define X_RTC_BGEV(label, var1, var2) \
    X_RTC_GETR(var1, R1), \
    X_RTC_GETR(var2, R2), \
    I_SUBR(R1, R2, R1), \
    M_BXZ(label), \
    M_BXF(label)

/**
 * Branch to given label if RTCMEM[var] < value
 * Uses R0, R3 for operation
 */
#define X_RTC_BLI(label, var, value) \
    X_RTC_GETR(var, R0), \
    M_BL(label, value)

/**
 * Branch to given label if RTCMEM[var] >= value
 * Uses R0, R3 for operation
 */
#define X_RTC_BGEI(label, var, value) \
    X_RTC_GETR(var, R0), \
    M_BGE(label, value)

/**
 * Branch to given label if RTCMEM[var1] == value
 * Uses R1 - R3 for operation
 */
#define X_RTC_BEQI(label, var, value) \
    X_RTC_GETR(var, R2), \
    X_BEQ(label, R2, value)

/**
 * Branch to given label if RTCMEM[var1] != value
 * Uses R1 - R3 for operation
 */
#define X_RTC_BNEI(label, var, value) \
    X_RTC_GETR(var, R2), \
    X_BNE(label, R2, value)

/**
 * Branch to given label VAR_ULP_CALL_COUNT & mask != 0
 * Uses R0 for operation
 */
#define X_MASK_BNE(label, mask) \
    X_RTC_GETR(VAR_ULP_CALL_COUNT, R0), \
    I_ANDI(R0, R0, mask), \
    X_BGZ(label)


/**
 * Push register onto stack.
 * Uses R2 - R3 for operation
 */
#define X_STACK_PUSHR(reg) \
    I_MOVI(R3, VAR_STACK_PTR), \
    I_LD(R2, R3, 0), \
    I_ST(reg, R2, 0), \
    I_ADDI(R2, R2, 1), \
    I_ST(R2, R3, 0)

/**
 * Push constant onto stack.
 * Uses R2 - R3 for operation
 */
#define X_STACK_PUSHI(value) \
    I_MOVI(R2, value), \
    I_MOVI(R3, VAR_STACK_PTR), \
    I_LD(R3, R3, 0), \
    I_ST(R2, R3, 0), \
    I_ADDI(R3, R3, 1), \
    I_MOVI(R2, VAR_STACK_PTR), \
    I_ST(R3, R2, 0)

/**
 * Push content of RTCMEM[var] onto stack.
 * Uses R1 - R3 for operation
 */
#define X_STACK_PUSHV(var) \
    X_RTC_GETR(var, R1), \
    X_STACK_PUSHR(R1)

/**
 * Push element at given position from top of stack onto the stack.
 * Uses R2 - R3 for operation
 */
#define X_STACK_PUSHE(count) \
    I_MOVI(R3, VAR_STACK_PTR), \
    I_LD(R2, R3, 0), \
    I_SUBI(R2, R2, count+1), \
    I_LD(R3, R2, 0), \
    I_ADDI(R2, R2, count+1), \
    I_ST(R3, R2, 0), \
    I_MOVI(R3, VAR_STACK_PTR), \
    I_ADDI(R2, R2, 1), \
    I_ST(R2, R3, 0)

/**
 * Peek at an element from the top of the stack.
 * Uses R2 - R3 for operation
 */
#define X_STACK_PEEK(reg, count) \
    I_MOVI(R3, VAR_STACK_PTR), \
    I_LD(R2, R3, 0), \
    I_SUBI(R2, R2, count+1), \
    I_LD(reg, R2, 0)
  
/**
 * Update a specific element from the top of the stack.
 * Uses R2 - R3 for operation
 */
#define X_STACK_POKE(reg, count) \
    I_MOVI(R3, VAR_STACK_PTR), \
    I_LD(R2, R3, 0), \
    I_SUBI(R2, R2, count+1), \
    I_ST(reg, R2, 0)


/**
 * Pop a given number of elements off the top of the stack.
 * Uses R2 - R3 for operation
 */
#define X_STACK_POPI(count) \
    I_MOVI(R3, VAR_STACK_PTR), \
    I_LD(R2, R3, 0), \
    I_SUBI(R2, R2, count), \
    I_ST(R2, R3, 0)

/**
 * Pop a given number of elements off the top of the stack, returning last element in reg
 * Uses R2 - R3 for operation
 */
#define X_STACK_POP(reg, count) \
    X_STACK_POPI(count), \
    I_LD(reg, R2, 0)

/**
 * Helper for X_CALL() that auto-generates return address marker.
 * Uses R2 - R3 for operation
 */
#define __X_CALL(label, marker) \
    M_MOVL(R1, marker), \
    X_STACK_PUSHR(R1), \
    M_BX(label), \
  M_LABEL(marker) 

/**
 * Call subroutine addressed by label.
 * Uses R2 - R3 for operation
 */
#define X_CALL(label) \
    __X_CALL(label, LBL_MARKER+__LINE__)

/**
 * Return from subroutine.
 * Uses R1 - R3 for operation
 * - num_args: number of parameters for the target subroutine so the macro knows how to adjust the stack.
 */
#define X_RETURN(num_args) \
    X_STACK_PEEK(R1, 0), \
    X_STACK_POPI(num_args+1), \
    I_BXR(R1)

/**
 * Read GPIO pin value into R0.
 */
#define X_GPIO_GET(pin) \
    I_RD_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S+pin, RTC_GPIO_IN_NEXT_S+pin)

/**
 * Set GPIO pin value to target level
 */
#define X_GPIO_SET(pin, level) \
    I_WR_REG_BIT(RTC_GPIO_OUT_REG, RTC_GPIO_IN_NEXT_S+pin, level)

/**
 * Wait for MCU to become ready for wakeup
 */
#define __X_WAKE_WAIT(marker) \
  M_LABEL(marker), \
    I_RD_REG(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_RDY_FOR_WAKEUP_S, RTC_CNTL_RDY_FOR_WAKEUP_S), \
    M_BL(marker, 1), \

/**
 * Wake up MCU and halt ULP
 * 
 * Ideally, should call:
 * 
 *    __X_WAKE_WAIT(LBL_MARKER+__LINE__),
 * 
 * before I_WAKE(), but doing that causes WDT reset every few hours.
 * 
 * So removing this until there is more clarity.
 */
#define X_WAKE() \
    I_WAKE(), \
    I_HALT()

/**
 * R0 = R0 + ADC reading from GPIO32
 */
#define X_ADC_HELPER() \
    I_ADC(R1, 0, VDD_CHANNEL), \
    I_ADDR(R0, R0, R1)

/**
 * Sum 8 x ADC readings from GPIO32 and store result in R0
 * Uses R1 for operation
 */
#define X_ADC_SUM() \
    I_MOVI(R0, 0), \
    X_ADC_HELPER(), \
    X_ADC_HELPER(), \
    X_ADC_HELPER(), \
    X_ADC_HELPER(), \
    X_ADC_HELPER(), \
    X_ADC_HELPER(), \
    X_ADC_HELPER(), \
    X_ADC_HELPER()

/*
 * Toggle value of VAR_TICKPIN between 0 and 1
 * Uses R2 - R3 for operation
 */
#define X_FLIP_TICKPIN() \
    X_RTC_GETR(VAR_TICKPIN, R2), \
    I_ADDI(R2, R2, 1), \
    I_ANDI(R2, R2, 1), \
    X_RTC_SETR(VAR_TICKPIN, R2)

/**
 * Generate a 100us forward tick PWM on tickpin 1
 */
#define X_NORM_TICKPIN1() \
    X_GPIO_SET(TICKPIN1, 1), \
    I_DELAY(NORM_TICK_ON_US*8), \
    X_GPIO_SET(TICKPIN1, 0), \
    I_DELAY((100-NORM_TICK_ON_US)*8)

/**
 * Generate a 100us forward tick PWM on tickpin 2
 */
#define X_NORM_TICKPIN2() \
    X_GPIO_SET(TICKPIN2, 1), \
    I_DELAY(NORM_TICK_ON_US*8), \
    X_GPIO_SET(TICKPIN2, 0), \
    I_DELAY((100-NORM_TICK_ON_US)*8)

/**
 * Generate a 100us forward tick PWM on tickpin 1
 */
#define X_FWD_TICKPIN1() \
    X_GPIO_SET(TICKPIN1, 1), \
    I_DELAY(FWD_TICK_ON_US*8), \
    X_GPIO_SET(TICKPIN1, 0), \
    I_DELAY((100-FWD_TICK_ON_US)*8)

/**
 * Generate a 100us forward tick PWM on tickpin 2
 */
#define X_FWD_TICKPIN2() \
    X_GPIO_SET(TICKPIN2, 1), \
    I_DELAY(FWD_TICK_ON_US*8), \
    X_GPIO_SET(TICKPIN2, 0), \
    I_DELAY((100-FWD_TICK_ON_US)*8)

/**
 * Generate a 100us reverse tick PWM on tickpin 2
 */
#define X_REV_TICKPIN1() \
    X_GPIO_SET(TICKPIN1, 1), \
    I_DELAY(REV_TICKA_ON_US*8), \
    X_GPIO_SET(TICKPIN1, 0), \
    I_DELAY((100-REV_TICKA_ON_US)*8)

/**
 * Generate a 100us reverse tick PWM on tickpin 2
 */
#define X_REV_TICKPIN2() \
    X_GPIO_SET(TICKPIN2, 1), \
    I_DELAY(REV_TICKA_ON_US*8), \
    X_GPIO_SET(TICKPIN2, 0), \
    I_DELAY((100-REV_TICKA_ON_US)*8)

/**
 * Generate a 100us reverse tick PWM on tickpin 2
 */
#define X_REV_TICKPIN1_B() \
    X_GPIO_SET(TICKPIN1, 1), \
    I_DELAY(REV_TICKB_ON_US*8), \
    X_GPIO_SET(TICKPIN1, 0), \
    I_DELAY((100-REV_TICKB_ON_US)*8)

/**
 * Generate a 100us reverse tick PWM on tickpin 2
 */
#define X_REV_TICKPIN2_B() \
    X_GPIO_SET(TICKPIN2, 1), \
    I_DELAY(REV_TICKB_ON_US*8), \
    X_GPIO_SET(TICKPIN2, 0), \
    I_DELAY((100-REV_TICKB_ON_US)*8)

/**
 * Helper function for X_PWM() 
 */
#define __X_PWM(tickpin, time, marker) \
    I_MOVI(R0, time), \
  M_LABEL(marker), \
    I_STAGE_RST(), \
  M_LABEL(marker+LBL_MARKER_NEXT), \
    X_##tickpin(), \
    I_STAGE_INC(1), \
    M_BSLT(marker+LBL_MARKER_NEXT, 10), \
    I_SUBI(R0, R0, 1), \
    M_BGE(marker, 1)

/**
 * Generate a PWM waveform of a certain length of time.
 * Uses R0 for operation
 * - tickpin: FWD_TICKPIN1, FWD_TICKPIN2, REV_TICKPIN1, REV_TICKPIN1
 * - time: length in msecs
 */
#define X_PWM(tickpin, time) \
    __X_PWM(tickpin, time, LBL_MARKER+__LINE__)



/**
 *  Helper function for __X_TICK()
 */
#define __X_TICK2(tickpin, duty) \
    X_GPIO_SET(tickpin, 1), \
    I_DELAY(duty*8), \
    X_GPIO_SET(tickpin, 0), \
    I_DELAY((100-duty)*8)

/**
 * Helper function for X_TICK() 
 */
#define __X_TICK(tickpin, duty, time, marker) \
    I_MOVI(R0, time), \
  M_LABEL(marker), \
    I_STAGE_RST(), \
  M_LABEL(marker+LBL_MARKER_NEXT), \
    __X_TICK2(tickpin, duty), \
    I_STAGE_INC(1), \
    M_BSLT(marker+LBL_MARKER_NEXT, 10), \
    I_SUBI(R0, R0, 1), \
    M_BGE(marker, 1)

/**
 * Generate a PWM waveform of a certain length of time.
 * Uses R0 for operation
 * - tickpin: TICKPIN1, TICKPIN2
 * - duty: NORM_TICK_ON_US, FWD_TICK_ON_US, REV_TICK_ON_US etc.
 * - time: length in msecs
 */
#define X_TICK(tickpin, duty, time) \
    __X_TICK(tickpin, duty, time, LBL_MARKER+__LINE__)

/**
 * Helper function for X_DELAY_MS() 
 */
#define __X_DELAY_MS(time, marker) \
    I_STAGE_RST(), \
  M_LABEL(marker), \
    I_STAGE_INC(1), \
    M_BSGE(marker+LBL_MARKER_NEXT, time), \
    I_DELAY(8000), \
    M_BX(marker), \
  M_LABEL(marker+LBL_MARKER_NEXT)

/**
 * Delay for given length of time in msecs.
 */
#define X_DELAY_MS(time) \
    __X_DELAY_MS(time, LBL_MARKER+__LINE__)

/**
 * VAR_ULP_CALL_COUNT = (VAR_ULP_CALL_COUNT + 1) % 8
 * Uses R3 for operation
 * R0 holds the final value of the variable.
 */
#define X_INC_ULP_CALL_COUNT() \
    X_RTC_INC(VAR_ULP_CALL_COUNT), \
    I_ANDI(R0, R0, 7), \
    X_RTC_SETR(VAR_ULP_CALL_COUNT, R0)

/**
 * Left-shift reg1 by the number of 1s in lower 4-bits of reg2
 * Uses R3 for operation
 */
#define X_LSHIFT(reg1, reg2) \
    I_MOVR(R3, reg2), \
    I_ANDI(reg2, reg2, 0x1), \
    I_LSHR(reg1, reg1, reg2), \
    I_MOVR(reg2, R3), \
    I_ANDI(reg2, reg2, 0x2), \
    I_RSHI(reg2, reg2, 1), \
    I_LSHR(reg1, reg1, reg2), \
    I_MOVR(reg2, R3), \
    I_ANDI(reg2, reg2, 0x4), \
    I_RSHI(reg2, reg2, 2), \
    I_LSHR(reg1, reg1, reg2), \
    I_MOVR(reg2, R3), \
    I_ANDI(reg2, reg2, 0x8), \
    I_RSHI(reg2, reg2, 3), \
    I_LSHR(reg1, reg1, reg2)

