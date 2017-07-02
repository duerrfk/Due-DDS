/**
 * This file is part of Due-DDS.
 * 
 * Copyright 2017 Frank Duerr
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// We compile for Arduino Due featuring a SAM3X8E.
#include <system_sam3x.h>

#include <math.h>

//
// Set the following definitions to define the output frequency and signal
// time resolution.
//

// The desired signal frequency [Hz].
#define F_SIGNAL 1000

// Size of phase-to-value table (must be a power of 2).
#define TABLE_SIZE 1024
 
//
// The following definitions usually do not need to be changed.
//

// Master clock frequency [Hz].
// Arduino Due runs at 84 MHz.
#define F_MASTER 84000000UL

// The timer runs with the minimum pre-scaler value of 2. 
#define F_TIMER (F_MASTER/2)

// Mask for fast modulo calculations.
#define TABLE_MODMASK (TABLE_SIZE-1)

// TIOA line of Timer Counter 0, Channel 0 is on Port B, Pin B25, Peripheral B.
#define PORT_TIOA0 PIOB
#define PIN_TIOA0 PIO_PB25
#define AB_TIOA0 PIO_ABSR_P25

// Phase table.
uint16_t phase_table[TABLE_SIZE];

/**
 * Load one period of a sine wave into the phase table.
 */
static void setup_phase_table_sine()
{
     double pstride = 2.0*M_PI/TABLE_SIZE;

     for (unsigned int i = 0; i < TABLE_SIZE; i++)
	  phase_table[i] = (uint16_t) (2047.0*sin(pstride*i) + 2048.0);
}

/**
 * Load one period of a saw-tooth wave into the phase table.
 */
static void setup_phase_table_saw()
{
     double y = 4095.0/TABLE_SIZE;
     for (unsigned int i = 0; i < TABLE_SIZE; i++)
	  phase_table[i] = (uint16_t) (y*i);
}

/**
 * Configuration of Timer Counter 0 (TC0), Channel 0 for automatic triggering 
 * digital-analog conversion via TIOA0. 
 *
 * This function will also start the timer after configuration.
 */
static void setup_dac_trigger_timer()
{
     volatile uint32_t timer_status __attribute__((unused));

     // Enable TC0/Channel0 with the Power Management Controller (PMC).
     PMC->PMC_PCER0 = (1UL << ID_TC0);
    
     // Disable clock via Channel Control Register (CCR) while configuring.
     TC0->TC_CHANNEL[0].TC_CCR = TC_CCR_CLKDIS;
 
     // Set TIOA0 pin to peripheral function via PIO Disable Register (PDR).
     PORT_TIOA0->PIO_PDR = PIN_TIOA0;
     // Select corresponding A/B peripheral via AB Select Register (ABSR).
     PORT_TIOA0->PIO_ABSR = AB_TIOA0;

     // Disable all interrupts.
     TC0->TC_CHANNEL[0].TC_IDR = 0xffffffff;
 
     // Clear timer status register by reading it.
     timer_status = TC0->TC_CHANNEL[0].TC_SR;

     // Set timer mode via Channel Mode Register (CMR).
     // - TC_CMR_CPCTRG: waveform mode
     // - TC_CMR_TCCLKS_TIMER_CLOCK1: run at MCLK/2 (84 MHz/2 = 42 MHz)
     // - TC_CMR_ACPA_TOGGLE: toogle TIOA on RA.
     // - TC_CMR_WAVSEL_UP_RC: counting upwards (not up/down) with
     //                        automatic trigger (counter clear) on RC
     TC0->TC_CHANNEL[0].TC_CMR = TC_CMR_WAVE | TC_CMR_TCCLKS_TIMER_CLOCK1 |
	  TC_CMR_ACPA_TOGGLE | TC_CMR_WAVSEL_UP_RC;

     // Set compare values.
     // - On RA, the state of TIOA is toggled. DAC is triggered by a rising 
     //   edge on TIOA. Thus, D/A conversion is triggered every second cycle,
     //   and we need to divide by 2 to match the desired DAC frequency.
     // - On RC (= RA), the counter is reset. 
     uint32_t cmpvalue = (uint32_t) (((double) (F_TIMER)/(F_SIGNAL*TABLE_SIZE)
				      -1.0)/2.0 + 0.5);
     TC0->TC_CHANNEL[0].TC_RA = cmpvalue;
     TC0->TC_CHANNEL[0].TC_RC = cmpvalue;

     // Enable clock (CLKEN).
     TC0->TC_CHANNEL[0].TC_CCR = TC_CCR_CLKEN;

     // Reset and start timer counter by software trigger (SWTRG).
     TC0->TC_CHANNEL[0].TC_CCR = TC_CCR_SWTRG;
}

/**
 * Setup Digital-to-Analog Converter (DAC), Channel 0.
 */
static void setup_dac()
{
     // Enable peripheral clock for DAC with Power Management Controller (PMC).
     // ID_DACC is >= 32, thus, we need to write to PMC_PCER1.
     PMC->PMC_PCER1 = (1UL << (ID_DACC-32));

     // Set DAC mode via DAC Mode Register (MR)
     // - DACC_MR_TRGEN_EN: enable external trigger mode
     // - DACC_MR_TRGSEL(0x01): TIOA output of Timer Counter Channel 0
     // - DACC_MR_WORD_HALF: half word (16 bit) transfer of conversion data
     // - DACC_MR_USER_SEL_CHANNEL0: select DAC channel 0 
     // - DACC_MR_REFRESH(value): refresh period for refreshing the signal. 
     //   After 20 us, the signal level will decrease and needs to be refreshed.
     //   Refresh period = 1024*value/DACC_Clock with DACC_Clock = 42 MHz
     //   (master clock/2).
     DACC->DACC_MR = DACC_MR_TRGEN_EN | DACC_MR_TRGSEL(0x01) | 
	  DACC_MR_WORD_HALF | DACC_MR_USER_SEL_CHANNEL0 | 
	  DACC_MR_REFRESH(1);

     // Enable Channel 0 via Channel Enable Register (CHER).
     DACC->DACC_CHER = DACC_CHER_CH0;
}

int main()
{
     // CMSIS: initialize system (clock, etc.)
     SystemInit();

     // Disable watchdog.
     WDT->WDT_MR = WDT_MR_WDDIS;

     // Load a sine wave into the phase table.
     // Change this for any other type of signal.
     setup_phase_table_sine();

     // Setup DAC.
     setup_dac();

     // Setup timer to periodically trigger DAC.
     // This will also start the timer.
     setup_dac_trigger_timer();

     size_t itable = 0; 
     // Continuously fill DAC FIFO buffer with values to be converted.
     // Concurrently, the timer triggers conversions, and DAC will consume 
     // values from the FIFO buffer.
     while (1) {
	  // Busy-wait for DAC to become ready to accept conversion requests 
	  // by checking Transmit Ready (TXRDY) flag of DAC Interrupt Status 
	  // Register (ISR).
	  while ( (DACC->DACC_ISR&DACC_ISR_TXRDY) == 0 );

	  // DAC is now ready to accept new conversion requests. 

	  // Data to be converted is written to DAC Conversion Data Register
	  // (CDR). Requests that cannot be handled immediately go into FIFO 
	  // buffer.
	  DACC->DACC_CDR = phase_table[itable];
	  itable = ((itable+1)&TABLE_MODMASK);
     }
}
