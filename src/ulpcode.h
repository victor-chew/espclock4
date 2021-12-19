/*
 * ulpcode.h
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

#include "ulpdefs.h"

const ulp_insn_t ulp_code[] = {
  /////////////////////////////////////////////////////////////////////////////////
  // Stress test logic. If this is included, normal code will not be executed.
  /////////////////////////////////////////////////////////////////////////////////
#ifdef STRESS_TEST
  M_LABEL(LBL_STRESS_TEST),
    // Check pause flag
    X_RTC_BEQI(LBL_STRESS_TEST+LBL_NEXT, VAR_PAUSE_CLOCK, 0),
    I_HALT(),
  M_LABEL(LBL_STRESS_TEST+LBL_NEXT),
    // Check current direction
    X_RTC_BEQI(LBL_STRESS_TEST+LBL_NEXT*2, VAR_TICK_ACTION, TICK_FWD),
    X_RTC_BEQI(LBL_STRESS_TEST+LBL_NEXT*3, VAR_TICK_ACTION, TICK_REV),
    // Normal tick
    X_RTC_GETR(VAR_TICK_DELAY, R0), I_MOVI(R1, NORM_COUNT_MASK), X_LSHIFT(R0, R1), X_STACK_PUSHR(R0),
    X_MASK_BNE(LBL_STRESS_TEST+LBL_NEXT*4, NORM_COUNT_MASK),
    X_CALL(LBL_FN_NORM_TICK),
    M_BX(LBL_STRESS_TEST+LBL_NEXT*5),
    // Forward tick
  M_LABEL(LBL_STRESS_TEST+LBL_NEXT*2),
    X_RTC_GETR(VAR_TICK_DELAY, R0), I_MOVI(R1, FWD_COUNT_MASK), X_LSHIFT(R0, R1), X_STACK_PUSHR(R0),
    X_MASK_BNE(LBL_STRESS_TEST+LBL_NEXT*4, FWD_COUNT_MASK),
    X_CALL(LBL_FN_FWD_TICK),
    M_BX(LBL_STRESS_TEST+LBL_NEXT*5),
    // Reverse tick A
  M_LABEL(LBL_STRESS_TEST+LBL_NEXT*3),
    X_RTC_BLI(LBL_STRESS_TEST+LBL_NEXT*7, VAR_CLK_SS, REV_TICKA_LO),
    X_RTC_BGEI(LBL_STRESS_TEST+LBL_NEXT*7, VAR_CLK_SS, REV_TICKA_HI),
    X_RTC_GETR(VAR_TICK_DELAY, R0), I_MOVI(R1, REV_COUNT_MASK), X_LSHIFT(R0, R1), X_STACK_PUSHR(R0),
    X_MASK_BNE(LBL_STRESS_TEST+LBL_NEXT*4, REV_COUNT_MASK),
    X_CALL(LBL_FN_REV_TICKA),
    M_BX(LBL_STRESS_TEST+LBL_NEXT*5),
    // Reverse tick B
  M_LABEL(LBL_STRESS_TEST+LBL_NEXT*7),
    X_RTC_GETR(VAR_TICK_DELAY, R0), I_MOVI(R1, REV_COUNT_MASK), X_LSHIFT(R0, R1), X_STACK_PUSHR(R0),
    X_MASK_BNE(LBL_STRESS_TEST+LBL_NEXT*4, REV_COUNT_MASK),
    X_CALL(LBL_FN_REV_TICKB),
    M_BX(LBL_STRESS_TEST+LBL_NEXT*5),
    // No-op - filler wait
  M_LABEL(LBL_STRESS_TEST+LBL_NEXT*4),
    X_DELAY_MS(MAX_PULSE_MS),
    // Pause when number of ticks in VAR_TICK_DELAY is done
  M_LABEL(LBL_STRESS_TEST+LBL_NEXT*5),
    X_STACK_POP(R1, 1),
    X_RTC_INC(VAR_ULP_CALL_COUNT),
    I_SUBR(R0, R1, R0),
    M_BXZ(LBL_STRESS_TEST+LBL_NEXT*6),
    I_HALT(),
  M_LABEL(LBL_STRESS_TEST+LBL_NEXT*6),
    // Wake up main core
    X_RTC_SETI(VAR_ULP_CALL_COUNT, 0),
    X_RTC_SETI(VAR_PAUSE_CLOCK, 1),
    X_WAKE(),
#else // !STRESS_TEST
  /////////////////////////////////////////////////////////////////////////////////
  // Check VDD every second via ADC on voltage divider
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_CHECK_VDD),
    // Only check VDD every sec 
    X_MASK_BNE(LBL_CHECK_VDD+LBL_NEXT*9, NORM_COUNT_MASK),
    // Read VDD via ADC
    X_ADC_SUM(),                                                  // R0 = ADC_SUM                 
    I_RSHI(R0, R0, 3),                                            // R0 = ADC_SUM /  8
    X_STACK_PUSHR(R0),
    X_RTC_BLV(LBL_CHECK_VDD+LBL_NEXT, VAR_ADC_VDD, VAR_ADC_VDDL),
    // If (old ADC_VDD >= ADC_VDDL), check that we are going low and need to pause clock
    X_STACK_POP(R0, 1),
    X_RTC_SETR(VAR_ADC_VDD, R0),
    X_RTC_BGEV(LBL_CHECK_VDD+LBL_NEXT*9, VAR_ADC_VDD, VAR_ADC_VDDL),
    X_RTC_SETI(VAR_PAUSE_CLOCK, 1),
    M_BX(LBL_COMMON_HALT),
    // If (old ADC_VDD < ADC_VDDL), check that we are going high and need to start clock again
  M_LABEL(LBL_CHECK_VDD+LBL_NEXT),
    X_STACK_POP(R0, 1),
    X_RTC_SETR(VAR_ADC_VDD, R0),
    X_RTC_BGEV(LBL_CHECK_VDD+LBL_NEXT*2, VAR_ADC_VDD, VAR_ADC_VDDH),
    M_BX(LBL_COMMON_HALT),
    // Restart clock after power is restored
  M_LABEL(LBL_CHECK_VDD+LBL_NEXT*2),
    M_BX(LBL_COMMON_RESTART_CLOCK),
  M_LABEL(LBL_CHECK_VDD+LBL_NEXT*9),
  /////////////////////////////////////////////////////////////////////////////////
  // Check reset button
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_CHECK_RESETBTN),
    X_GPIO_GET(RESETBTN_PIN),                                     // Active LOW
    X_BGZ(LBL_CHECK_RESETBTN+LBL_NEXT*9),
    X_DELAY_MS(50),                                               // Debounce check
    X_GPIO_GET(RESETBTN_PIN),
    X_BGZ(LBL_CHECK_RESETBTN+LBL_NEXT*9),                         // At this point, we are certain reset button has been pressed
    X_RTC_BEQI(LBL_CHECK_RESETBTN+LBL_NEXT,   VAR_PAUSE_CLOCK, 0),// VAR_PAUSE_CLOCK == 0
    X_RTC_BEQI(LBL_CHECK_RESETBTN+LBL_NEXT*2, VAR_PAUSE_CLOCK, 2),// VAR_PAUSE_CLOCK == 2
    M_BX(LBL_COMMON_HALT),                                        // VAR_PAUSE_CLOCK == 1
    // Reset button press detected when VAR_PAUSE_CLOCK == 1; pause clock and inform main core
  M_LABEL(LBL_CHECK_RESETBTN+LBL_NEXT),
    X_RTC_SETI(VAR_PAUSE_CLOCK, 1),
    X_RTC_SETI(VAR_WAKE_REASON, WAKE_RESET_BUTTON),
    M_BX(LBL_COMMON_WAKE), 
    // Reset button press detected when VAR_PAUSE_CLOCK == 2; restart clock
  M_LABEL(LBL_CHECK_RESETBTN+LBL_NEXT*2),
    X_DELAY_MS(50),
    X_GPIO_GET(RESETBTN_PIN),
    X_BZ(LBL_CHECK_RESETBTN+LBL_NEXT*2),                          // Wait for reset button to be released
    M_BX(LBL_COMMON_RESTART_CLOCK),
  M_LABEL(LBL_CHECK_RESETBTN+LBL_NEXT*9),
  /////////////////////////////////////////////////////////////////////////////////
  // Check whether clock is paused
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_CHECK_PAUSE_CLOCK),
    X_RTC_BEQI(LBL_CHECK_PAUSE_CLOCK+LBL_NEXT*9, VAR_PAUSE_CLOCK, 0),
    M_BX(LBL_COMMON_HALT), 
  M_LABEL(LBL_CHECK_PAUSE_CLOCK+LBL_NEXT*9),
  /////////////////////////////////////////////////////////////////////////////////
  // Perform tick action
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_DO_TICK_ACTION),
    // Only proceed if VAR_TICK_DELAY == 0; otherwise decrement delay counter, execute filler delay and halt
    X_RTC_BEQI(LBL_DO_TICK_ACTION+LBL_NEXT, VAR_TICK_DELAY, 0),
    X_RTC_DEC(VAR_TICK_DELAY),
    X_DELAY_MS(MAX_PULSE_MS),
    M_BX(LBL_COMMON_HALT), 
    // Decide which tick action to perform
  M_LABEL(LBL_DO_TICK_ACTION+LBL_NEXT),
    X_RTC_BEQI(LBL_DO_TICK_ACTION+LBL_NEXT*3, VAR_TICK_ACTION, TICK_FWD),
    X_RTC_BEQI(LBL_DO_TICK_ACTION+LBL_NEXT*4, VAR_TICK_ACTION, TICK_REV),
    // TICK_NORMAL
  M_LABEL(LBL_DO_TICK_ACTION+LBL_NEXT*2),
    X_MASK_BNE(LBL_DO_TICK_ACTION+LBL_NEXT*5, NORM_COUNT_MASK),   // Do not proceed if (VAR_ULP_CALL_COUNT & NORM_COUNT_MASK) != 0
    X_CALL(LBL_FN_NORM_TICK),                                     // Generate tick pulse
    M_BX(LBL_DO_TICK_ACTION+LBL_NEXT*6),
    // TICK_FWD
  M_LABEL(LBL_DO_TICK_ACTION+LBL_NEXT*3),
    X_MASK_BNE(LBL_DO_TICK_ACTION+LBL_NEXT*5, FWD_COUNT_MASK),    // Do not proceed if (VAR_ULP_CALL_COUNT & FWD_COUNT_MASK) != 0
    X_CALL(LBL_FN_FWD_TICK),                                      // Generate tick pulse
    M_BX(LBL_DO_TICK_ACTION+LBL_NEXT*6),
    // TICKA_REV
  M_LABEL(LBL_DO_TICK_ACTION+LBL_NEXT*4),
    X_RTC_BLI(LBL_DO_TICK_ACTION+LBL_NEXT*7, VAR_CLK_SS, REV_TICKA_LO),
    X_RTC_BGEI(LBL_DO_TICK_ACTION+LBL_NEXT*7, VAR_CLK_SS, REV_TICKA_HI),
    X_MASK_BNE(LBL_DO_TICK_ACTION+LBL_NEXT*5, REV_COUNT_MASK),    // Do not proceed if (VAR_ULP_CALL_COUNT & REV_COUNT_MASK) != 0
    X_CALL(LBL_FN_REV_TICKA),                                     // Generate tick pulse
    M_BX(LBL_DO_TICK_ACTION+LBL_NEXT*6),
    // TICKB_REV
  M_LABEL(LBL_DO_TICK_ACTION+LBL_NEXT*7),
    X_MASK_BNE(LBL_DO_TICK_ACTION+LBL_NEXT*5, REV_COUNT_MASK),    // Do not proceed if (VAR_ULP_CALL_COUNT & REV_COUNT_MASK) != 0
    X_CALL(LBL_FN_REV_TICKB),                                     // Generate tick pulse
    M_BX(LBL_DO_TICK_ACTION+LBL_NEXT*6),
    // Filler delay
  M_LABEL(LBL_DO_TICK_ACTION+LBL_NEXT*5),
    X_DELAY_MS(MAX_PULSE_MS),
    // If 1 second has passed, we need to update some counters and increment network time
  M_LABEL(LBL_DO_TICK_ACTION+LBL_NEXT*6),
    X_MASK_BNE(LBL_DO_TICK_ACTION+LBL_NEXT*9, NORM_COUNT_MASK), 
    X_STACK_PUSHI(VAR_NET_SS), X_STACK_PUSHI(VAR_NET_MM), X_STACK_PUSHI(VAR_NET_HH), X_CALL(LBL_FN_INC_CLOCK),
    X_RTC_INC(VAR_SLEEP_COUNT),
    X_RTC_BEQI(LBL_DO_TICK_ACTION+LBL_NEXT*9, VAR_UPDATE_PENDING, 0),
    X_RTC_DEC(VAR_UPDATE_PENDING),
    X_RTC_BNEI(LBL_DO_TICK_ACTION+LBL_NEXT*9, VAR_UPDATE_PENDING, 0),
    X_RTC_SETI(VAR_WAKE_REASON, WAKE_UPDATE_NETTIME),
    I_WAKE(),
  M_LABEL(LBL_DO_TICK_ACTION+LBL_NEXT*9),
  /////////////////////////////////////////////////////////////////////////////////
  // Based on clock and net time difference, decide on next tick action 
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_COMPUTE_TICK_ACTION),
    // Default next tick action is TICK_NORMAL
    X_RTC_SETV(VAR_PREV_TACTION, VAR_TICK_ACTION),
    X_RTC_SETI(VAR_TICK_ACTION, TICK_NORMAL),                     
    // If diff(clock, net) == 0 Then no change i.e. TICK_NORMAL
    X_CALL(LBL_FN_CALC_TIME_DIFF),                                
    X_BGZ(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*2),
    // Tick action - TICK_NORMAL
    X_RTC_BNEI(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*9, VAR_PREV_TACTION, TICK_REV),
    X_RTC_SETI(VAR_TICK_DELAY, ULP_CALL_PER_SEC/2),               // If previous tick action == TICK_REV, delay for 0.5sec
    M_BX(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*9),
  M_LABEL(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*2),
    // If diff(clock, net) >= threshold, Then TICK_REV
    I_MOVI(R1, (DIFF_THRESHOLD_HH<<12)|(DIFF_THRESHOLD_MM<<6)|DIFF_THRESHOLD_SS),
    I_SUBR(R0, R1, R0),
    M_BXZ(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*5),
    M_BXF(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*5),
    // Tick action = TICK_FWD
    X_RTC_BEQI(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*3, VAR_PREV_TACTION, TICK_FWD), // If previous tick action is TICK_FWD, then proceed
    X_RTC_GETR(VAR_DIFF_PACKED, R0),
    M_BL(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*9, TOLERANCE_SS),                 // Do not start TICK_FWD if ABS(diff(clock, net)) < tolerance
    I_SUBI(R0, R0, (11<<12)|(59<<6)|(TOLERANCE_SS+1)), 
    M_BL(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*9, TOLERANCE_SS),
    // Confirm tick action = TICK_FWD
  M_LABEL(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*3),
    X_RTC_BNEI(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*4, VAR_PREV_TACTION, TICK_REV),        
    X_RTC_SETI(VAR_TICK_DELAY, ULP_CALL_PER_SEC),                 // If previous tick action is TICK_REV, we need to set a delay due to change in direction
  M_LABEL(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*4),
    X_RTC_SETI(VAR_TICK_ACTION, TICK_FWD),
    X_RTC_SETI(VAR_DEBUG, TICK_FWD),
    M_BX(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*9),
    // Tick action = TICK_REV
  M_LABEL(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*5),
    X_RTC_BEQI(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*6, VAR_PREV_TACTION, TICK_REV), // If previous tick action is TICK_REV, then proceed
    X_RTC_GETR(VAR_DIFF_PACKED, R0),
    M_BL(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*9, TOLERANCE_SS),                 // Do not start TICK_REV if ABS(diff(clock, net)) < tolerance
    I_SUBI(R0, R0, (11<<12)|(59<<6)|(TOLERANCE_SS+1)), 
    M_BL(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*9, TOLERANCE_SS),
    // Confirm tick action = TICK_REV
  M_LABEL(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*6),
    X_RTC_BNEI(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*7, VAR_PREV_TACTION, TICK_FWD),        
    X_RTC_SETI(VAR_TICK_DELAY, ULP_CALL_PER_SEC),                 // If previous tick action is TICK_FWD, we need to set a delay due to change in direction
  M_LABEL(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*7),
    X_RTC_SETI(VAR_TICK_ACTION, TICK_REV),
    X_RTC_SETI(VAR_DEBUG, TICK_REV),
  M_LABEL(LBL_COMPUTE_TICK_ACTION+LBL_NEXT*9),
  /////////////////////////////////////////////////////////////////////////////////
  // Check whether we need to wake up MCU to tune the ULP timer
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_CHECK_TUNE_ULP_TIMER),
    X_RTC_BGEV(LBL_CHECK_TUNE_ULP_TIMER+LBL_NEXT, VAR_SLEEP_COUNT, VAR_SLEEP_INTERVAL),
    M_BX(LBL_COMMON_HALT),
  M_LABEL(LBL_CHECK_TUNE_ULP_TIMER+LBL_NEXT),
    X_RTC_BEQI(LBL_CHECK_TUNE_ULP_TIMER+LBL_NEXT*2, VAR_TICK_ACTION, TICK_NORMAL),
    X_RTC_GETR(VAR_SLEEP_INTERVAL, R0),                           // If clock is still catching up, extend VAR_SLEEP_INTERVAL by 5 mins
    I_ADDI(R0, R0, 5*60),
    X_RTC_SETR(VAR_SLEEP_INTERVAL, R0),
    M_BX(LBL_COMMON_HALT),
  M_LABEL(LBL_CHECK_TUNE_ULP_TIMER+LBL_NEXT*2),
    X_RTC_SETI(VAR_SLEEP_COUNT, 0),
    X_RTC_SETI(VAR_WAKE_REASON, WAKE_TUNE_ULP_TIMER),
    M_BX(LBL_COMMON_WAKE),
  /////////////////////////////////////////////////////////////////////////////////
  // Common exit point to reset counters and refresh network time
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_COMMON_RESTART_CLOCK),
    X_RTC_SETI(VAR_PAUSE_CLOCK, 0),
    X_RTC_SETI(VAR_ULP_CALL_COUNT, 0),
    X_RTC_SETI(VAR_SLEEP_COUNT, 0),                               
    X_RTC_SETI(VAR_WAKE_REASON, WAKE_UPDATE_NETTIME),
    X_WAKE(),
  /////////////////////////////////////////////////////////////////////////////////
  // Common exit point to update VAR_ULP_COUNT and halt
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_COMMON_HALT),
    X_INC_ULP_CALL_COUNT(),
    I_HALT(),
  /////////////////////////////////////////////////////////////////////////////////
  // Common exit point to update VAR_ULP_COUNT and wake main core
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_COMMON_WAKE),
    X_INC_ULP_CALL_COUNT(),
    X_WAKE(),
#endif // STRESS_TEST
  /////////////////////////////////////////////////////////////////////////////////
  // Subroutine - Generate a normal tick
  //   params - none
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_FN_NORM_TICK),
    // Generate pulse
    X_RTC_GETR(VAR_TICKPIN, R0),
    X_BGZ(LBL_FN_NORM_TICK+LBL_NEXT),
    // For tickpin 1
    X_TICK(TICKPIN1, NORM_TICK_ON_US, NORM_TICK_MS),
    M_BX(LBL_FN_NORM_TICK+LBL_NEXT*2),
    // For tickpin 2
  M_LABEL(LBL_FN_NORM_TICK+LBL_NEXT),
    X_TICK(TICKPIN2, NORM_TICK_ON_US, NORM_TICK_MS),
    // Flip tickpin
  M_LABEL(LBL_FN_NORM_TICK+LBL_NEXT*2),
    X_FLIP_TICKPIN(),
    // Filler delay until MAX_PULSE_MS has passed
    X_DELAY_MS(NORM_TICK_FILLER_MS),
    // Increment clock time
    X_STACK_PUSHI(VAR_CLK_SS), X_STACK_PUSHI(VAR_CLK_MM), X_STACK_PUSHI(VAR_CLK_HH), X_CALL(LBL_FN_INC_CLOCK),
    X_RETURN(0),
  /////////////////////////////////////////////////////////////////////////////////
  // Subroutine - Generate a forward tick
  //   params - none
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_FN_FWD_TICK),
    // Generate pulse
    X_RTC_GETR(VAR_TICKPIN, R0),
    X_BGZ(LBL_FN_FWD_TICK+LBL_NEXT),
    // For tickpin 1
    X_TICK(TICKPIN1, FWD_TICK_ON_US, FWD_TICK_MS),
    M_BX(LBL_FN_FWD_TICK+LBL_NEXT*2),
    // For tickpin 2
  M_LABEL(LBL_FN_FWD_TICK+LBL_NEXT),
    X_TICK(TICKPIN2, FWD_TICK_ON_US, FWD_TICK_MS),
    // Flip tickpin
  M_LABEL(LBL_FN_FWD_TICK+LBL_NEXT*2),
    X_FLIP_TICKPIN(),
    // Filler wait until MAX_PULSE_MS has passed
    X_DELAY_MS(FWD_TICK_FILLER_MS),
    // Increment clock time
    X_STACK_PUSHI(VAR_CLK_SS), X_STACK_PUSHI(VAR_CLK_MM), X_STACK_PUSHI(VAR_CLK_HH), X_CALL(LBL_FN_INC_CLOCK),
    X_RETURN(0),
  /////////////////////////////////////////////////////////////////////////////////
  // Subroutine - Generate a reverse tick (region A)
  //   params - none
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_FN_REV_TICKA),
    // Generate short pulse
    X_RTC_GETR(VAR_TICKPIN, R0),
    X_BGZ(LBL_FN_REV_TICKA+LBL_NEXT),
    // For tickpin 1
    X_TICK(TICKPIN1, REV_TICKA_ON_US, REV_TICKA_T1_MS),
    M_BX(LBL_FN_REV_TICKA+LBL_NEXT*2),
    // For tickpin 2
  M_LABEL(LBL_FN_REV_TICKA+LBL_NEXT),
    X_TICK(TICKPIN2, REV_TICKA_ON_US, REV_TICKA_T1_MS),
    // Delay and flip tickpin
  M_LABEL(LBL_FN_REV_TICKA+LBL_NEXT*2),
    X_DELAY_MS(REV_TICKA_T2_MS),
    X_FLIP_TICKPIN(),
    // Generate long pulse
    X_RTC_GETR(VAR_TICKPIN, R0),
    X_BGZ(LBL_FN_REV_TICKA+LBL_NEXT*3),
    // For tickpin 1
    X_TICK(TICKPIN1, REV_TICKA_ON_US, REV_TICKA_T3_MS),
    M_BX(LBL_FN_REV_TICKA+LBL_NEXT*4),
    // For tickpin 2
  M_LABEL(LBL_FN_REV_TICKA+LBL_NEXT*3),
    X_TICK(TICKPIN2, REV_TICKA_ON_US, REV_TICKA_T3_MS),
    // Filler delay until MAX_PULSE_MS has passed
  M_LABEL(LBL_FN_REV_TICKA+LBL_NEXT*4),
    X_DELAY_MS(REV_TICKA_FILLER_MS),
    // Decrement clock time
    X_STACK_PUSHI(VAR_CLK_SS), X_STACK_PUSHI(VAR_CLK_MM), X_STACK_PUSHI(VAR_CLK_HH), X_CALL(LBL_FN_DEC_CLOCK),
    X_RETURN(0),
  /////////////////////////////////////////////////////////////////////////////////
  // Subroutine - Generate a reverse tick (region B)
  //   params - none
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_FN_REV_TICKB),
    // Generate short pulse
    X_RTC_GETR(VAR_TICKPIN, R0),
    X_BGZ(LBL_FN_REV_TICKB+LBL_NEXT),
    // For tickpin 1
    X_TICK(TICKPIN1, REV_TICKB_ON_US, REV_TICKB_T1_MS),
    M_BX(LBL_FN_REV_TICKB+LBL_NEXT*2),
    // For tickpin 2
  M_LABEL(LBL_FN_REV_TICKB+LBL_NEXT),
    X_TICK(TICKPIN2, REV_TICKB_ON_US, REV_TICKB_T1_MS),
    // Delay and flip tickpin
  M_LABEL(LBL_FN_REV_TICKB+LBL_NEXT*2),
    X_DELAY_MS(REV_TICKB_T2_MS),
    X_FLIP_TICKPIN(),
    // Generate long pulse
    X_RTC_GETR(VAR_TICKPIN, R0),
    X_BGZ(LBL_FN_REV_TICKB+LBL_NEXT*3),
    // For tickpin 1
    X_TICK(TICKPIN1, REV_TICKB_ON_US, REV_TICKB_T3_MS),
    M_BX(LBL_FN_REV_TICKB+LBL_NEXT*4),
    // For tickpin 2
  M_LABEL(LBL_FN_REV_TICKB+LBL_NEXT*3),
    X_TICK(TICKPIN2, REV_TICKB_ON_US, REV_TICKB_T3_MS),
    // Filler delay until MAX_PULSE_MS has passed
  M_LABEL(LBL_FN_REV_TICKB+LBL_NEXT*4),
    X_DELAY_MS(REV_TICKB_FILLER_MS),
    // Decrement clock time
    X_STACK_PUSHI(VAR_CLK_SS), X_STACK_PUSHI(VAR_CLK_MM), X_STACK_PUSHI(VAR_CLK_HH), X_CALL(LBL_FN_DEC_CLOCK),
    X_RETURN(0),
  /////////////////////////////////////////////////////////////////////////////////
  // Subroutine - Increment HH:MM:SS
  //   params1 - VAR_CLOCK_HH or VAR_NET_SS
  //   params2 - VAR_CLOCK_MM or VAR_NET_MM
  //   params3 - VAR_CLOCK_SS or VAR_NET_SS
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_FN_INC_CLOCK),
    // Inc second
    X_STACK_PEEK(R0, 3),
    I_MOVR(R3, R0),
    I_LD(R0, R3, 0),
    I_ADDI(R0, R0, 1),
    M_BGE(LBL_FN_INC_CLOCK+LBL_NEXT, 60),
    I_ST(R0, R3, 0),
    M_BX(LBL_FN_INC_CLOCK+LBL_NEXT*3),
    // Inc minute
  M_LABEL(LBL_FN_INC_CLOCK+LBL_NEXT),
    I_MOVI(R0, 0),
    I_ST(R0, R3, 0),
    X_STACK_PEEK(R0, 2),
    I_MOVR(R3, R0),
    I_LD(R0, R3, 0),
    I_ADDI(R0, R0, 1),
    M_BGE(LBL_FN_INC_CLOCK+LBL_NEXT*2, 60),
    I_ST(R0, R3, 0),
    M_BX(LBL_FN_INC_CLOCK+LBL_NEXT*3),
    // Inc hour
  M_LABEL(LBL_FN_INC_CLOCK+LBL_NEXT*2),
    I_MOVI(R0, 0),
    I_ST(R0, R3, 0),
    X_STACK_PEEK(R0, 1),
    I_MOVR(R3, R0),
    I_LD(R0, R3, 0),
    I_ADDI(R0, R0, 1),
    M_BL(LBL_FN_INC_CLOCK+LBL_NEXT*3, 12),
    I_MOVI(R0, 0), 
  M_LABEL(LBL_FN_INC_CLOCK+LBL_NEXT*3),
    I_ST(R0, R3, 0),
    X_RETURN(3),
  /////////////////////////////////////////////////////////////////////////////////
  // Subroutine - Decrement HH:MM:SS
  //   params1 - VAR_CLOCK_HH or VAR_NET_SS
  //   params2 - VAR_CLOCK_MM or VAR_NET_MM
  //   params3 - VAR_CLOCK_SS or VAR_NET_SS
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_FN_DEC_CLOCK),
    // Dec second
    X_STACK_PEEK(R0, 3),
    I_MOVR(R3, R0),
    I_LD(R0, R3, 0),
    X_BZ(LBL_FN_DEC_CLOCK+LBL_NEXT),
    I_SUBI(R0, R0, 1),
    I_ST(R0, R3, 0),
    M_BX(LBL_FN_DEC_CLOCK+LBL_NEXT*4),
    // Dec minute
  M_LABEL(LBL_FN_DEC_CLOCK+LBL_NEXT),
    I_MOVI(R0, 59),
    I_ST(R0, R3, 0),
    X_STACK_PEEK(R0, 2),
    I_MOVR(R3, R0),
    I_LD(R0, R3, 0),
    X_BZ(LBL_FN_DEC_CLOCK+LBL_NEXT*2),
    I_SUBI(R0, R0, 1),
    I_ST(R0, R3, 0),
    M_BX(LBL_FN_DEC_CLOCK+LBL_NEXT*4),
    // Dec hour
  M_LABEL(LBL_FN_DEC_CLOCK+LBL_NEXT*2),
    I_MOVI(R0, 59),
    I_ST(R0, R3, 0),
    X_STACK_PEEK(R0, 1),
    I_MOVR(R3, R0),
    I_LD(R0, R3, 0),
    X_BZ(LBL_FN_DEC_CLOCK+LBL_NEXT*3),
    I_SUBI(R0, R0, 1),
    I_ST(R0, R3, 0),
    M_BX(LBL_FN_DEC_CLOCK+LBL_NEXT*4),
  M_LABEL(LBL_FN_DEC_CLOCK+LBL_NEXT*3),
    I_MOVI(R0, 11),
    I_ST(R0, R3, 0),
  M_LABEL(LBL_FN_DEC_CLOCK+LBL_NEXT*4),
    X_RETURN(3),
  /////////////////////////////////////////////////////////////////////////////////
  // Subroutine - Calculate time required to forward clock time to net time
  //   params - none
  //   result stored in VAR_DIFF_HH, VAR_DIFF_MNM, VAR_DIFF_SS, VAR_DIFF_PACKED
  //   R0 contains VAR_DIFF_PACKED on return
  /////////////////////////////////////////////////////////////////////////////////
  M_LABEL(LBL_FN_CALC_TIME_DIFF),
    // Copy VAR_CLK_HH => VAR_DIFF_HH, VAR_CLK_MM => VAR_DIFF_MM
    X_RTC_GETR(VAR_CLK_MM, R0), X_RTC_SETR(VAR_DIFF_MM, R0),
    X_RTC_GETR(VAR_CLK_HH, R0), X_RTC_SETR(VAR_DIFF_HH, R0),                                   
    // VAR_DIFF_SS = VAR_NET_SS - VAR_CLK_SS
    X_RTC_GETR(VAR_CLK_SS, R0),
    X_RTC_GETR(VAR_NET_SS, R1),
    I_SUBR(R0, R1, R0),
    // If VAR_DIFF_SS < 0 Then Branch
    M_BXF(LBL_FN_CALC_TIME_DIFF+LBL_NEXT),
    // Otherwise, update VAR_DIFF_SS 
    X_RTC_SETR(VAR_DIFF_SS, R0),
    M_BX(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*2),
    // Handle VAR_DIFF_SS < 0
  M_LABEL(LBL_FN_CALC_TIME_DIFF+LBL_NEXT),
    // VAR_DIFF_SS += 60
    I_ADDI(R0, R0, 60),
    X_RTC_SETR(VAR_DIFF_SS, R0),
    // VAR_CLK_MM += 1
    X_RTC_GETR(VAR_DIFF_MM, R0),
    I_ADDI(R0, R0, 1),
    X_RTC_SETR(VAR_DIFF_MM, R0),
    // If VAR_CLK_MM < 60 Then Branch
    M_BL(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*2, 60),
    // VAR_CLK_MM = 0
    X_RTC_SETI(VAR_DIFF_MM, 0),
    // VAR_CLK_HH += 1
    X_RTC_INC(VAR_DIFF_HH),
  M_LABEL(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*2),
    // VAR_DIFF_MM = VAR_NET_MM - VAR_CLK_MM
    X_RTC_GETR(VAR_DIFF_MM, R0),
    X_RTC_GETR(VAR_NET_MM, R1),
    I_SUBR(R0, R1, R0),
    // If VAR_DIFF_MM < 0 Then Branch
    M_BXF(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*3),
    // Otherwise, update VAR_DIFF_MM 
    X_RTC_SETR(VAR_DIFF_MM, R0),
    M_BX(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*4),
    // Handle VAR_DIFF_MM < 0
  M_LABEL(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*3),
    // VAR_DIFF_MM += 60
    I_ADDI(R0, R0, 60),
    X_RTC_SETR(VAR_DIFF_MM, R0),
    // VAR_DIFF_HH += 1
    X_RTC_INC(VAR_DIFF_HH),
  M_LABEL(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*4),
    // If VAR_CLK_HH >= 12 Then VAR_CLK_HH -= 12
    X_RTC_GETR(VAR_DIFF_HH, R0),
    M_BL(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*5, 12),
    I_SUBI(R0, R0, 12),
  M_LABEL(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*5),
    // VAR_DIFF_HH = VAR_NET_HH - VAR_CLK_HH
    X_RTC_GETR(VAR_NET_HH, R1),
    I_SUBR(R0, R1, R0),
    // If VAR_DIFF_HH < 0 Then VAR_DIFF_HH += 12
    M_BXF(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*6),
    M_BX(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*7),
  M_LABEL(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*6),
    I_ADDI(R0, R0, 12),
  M_LABEL(LBL_FN_CALC_TIME_DIFF+LBL_NEXT*7),
    X_RTC_SETR(VAR_DIFF_HH, R0),
    // Compute packed format
    X_RTC_GETR(VAR_DIFF_SS, R0), 
    X_RTC_GETR(VAR_DIFF_MM, R2),
    I_LSHI(R2, R2, 6), 
    I_ORR(R0, R0, R2), 
    X_RTC_GETR(VAR_DIFF_HH, R2), 
    I_LSHI(R2, R2, 12), 
    I_ORR(R0, R0, R2),
    X_RTC_SETR(VAR_DIFF_PACKED, R0),
    // All done
    X_RETURN(0),
};
