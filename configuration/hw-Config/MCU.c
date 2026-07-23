/******************************************************************************
* File Name:   MCU.c
*
* Description: This file implements the PSOC Control C3 peripheral configuration
* used for 3 shunt foc algorithm.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2024-2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/
#include "Controller.h"
#include "HardwareIface.h"
#include "MCU.h"
#include "cycfg_mcdi.h"
#include "ParamConfig.h"
#include <math.h>
#if (MOTOR_CTRL_NO_OF_SCOPE_CHANNELS > 0) /*if scope is enabled*/
#include "probe_scope.h"
#endif

#define NTC_TABLE_SIZE 39
/* Counters values for LED operation */
#define CLR_LED    		(0)
#define SET_FULL_LED    (1000)
#define SET_DIM_LED 	(100)
#define FAULT_LED_LVL 	(200)

#define OCD_TH_RES 	    (10000)
#define OCD_TH_CAL 	    (0.0f)
#define OCD_I_AMP 	    (25.0f) /*Power Input Current Limit*/

// ADC reading to temperature lookup table (temperatures in Celsius for ntcg103jf103ft1s)
const int16_t ntc_adc_table[NTC_TABLE_SIZE] = {
	3890, 	// -40°C
	3831, 	// -35°C
	3758, 	// -30°C
	3672, 	// -25°C
	3570, 	// -20°C
	3451, 	// -15°C
	3315, 	// -10°C
	3164, 	// -5°C
	2997, 	// 0°C
	2819, 	// 5°C
	2631, 	// 10°C
	2438, 	// 15°C
	2242, 	// 20°C
	2048, 	// 25°C
	1859, 	// 30°C
	1678, 	// 35°C
	1508, 	// 40°C
	1349, 	// 45°C
	1203, 	// 50°C
	1070, 	// 55°C
	950, 	// 60°C
	842, 	// 65°C
	746, 	// 70°C
	661, 	// 75°C
	586, 	// 80°C
	519, 	// 85°C
	461, 	// 90°C
	409, 	// 95°C
	364, 	// 100°C
	324, 	// 105°C
	289, 	// 110°C
	259, 	// 115°C
	231, 	// 120°C
	208, 	// 125°C
	187, 	// 130°C
	168, 	// 135°C
	152, 	// 140°C
	137, 	// 145°C
	124 	// 150°C
};

const float ntc_temp_table[NTC_TABLE_SIZE] = {
    -40.00f,  // -40.00°C
    -35.00f,  // -35.00°C
    -30.00f,  // -30.00°C
    -25.00f,  // -25.00°C
    -20.00f,  // -20.00°C
    -15.00f,  // -15.00°C
    -10.00f,  // -10.00°C
    -5.00f,   // -5.00°C
    0.00f,    // 0.00°C
    5.00f,    // 5.00°C
    10.00f,  // 10.00°C
    15.00f,  // 15.00°C
    20.00f,  // 20.00°C
    25.00f,  // 25.00°C
    30.00f,  // 30.00°C
    35.00f,  // 35.00°C
    40.00f,  // 40.00°C
    45.00f,  // 45.00°C
    50.00f,  // 50.00°C
    55.00f,  // 55.00°C
    60.00f,  // 60.00°C
    65.00f,  // 65.00°C
    70.00f,  // 70.00°C
    75.00f,  // 75.00°C
    80.00f,  // 80.00°C
    85.00f,  // 85.00°C
    90.00f,  // 90.00°C
    95.00f,  // 95.00°C
    100.00f, // 100.00°C
    105.00f, // 105.00°C
    110.00f, // 110.00°C
    115.00f, // 115.00°C
    120.00f, // 120.00°C
    125.00f, // 125.00°C
    130.00f, // 130.00°C
    135.00f, // 135.00°C
    140.00f, // 140.00°C
    145.00f, // 145.00°C
    150.00f  // 150.00°C
};

uint32_t OCD_Current_to_PWM(float i_limit)
{
	uint32_t pwr_thpwm = OCD_TH_RES;
	float v_th = 0;

	v_th = 0.024 * i_limit - 0.0426 + OCD_TH_CAL;
	pwr_thpwm = (uint32_t)(v_th / 3.3 * OCD_TH_RES);

	return pwr_thpwm;
}

/******************************************************************************/
// Slow ISR callback generated by MCDI personality
void vres_0_motor_0_slow_callback(void);
/*
 * Fast ISR callback generated by MCDI personality
 * This callback is intended for high-frequency, time-critical handling.
 */
void vres_0_motor_0_fast_callback(void);

/******************************************************************************/
static inline void MCU_InitChipInfo(void)
{
    mc_info.chip_id = Cy_SysLib_GetDevice();
    mc_info.chip_id <<= 16;
    mc_info.chip_id |= Cy_SysLib_GetDeviceRevision();
}

static inline void MCU_PhaseUEnterHighZ(void)
{
    Cy_GPIO_SetHSIOM(PWMUL_PORT, PWMUL_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_SetHSIOM(PWMUH_PORT, PWMUH_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_Clr(PWMUL_PORT, PWMUL_NUM);
    Cy_GPIO_Clr(PWMUH_PORT, PWMUH_NUM);
}

static inline void MCU_PhaseUExitHighZ(void)
{
    Cy_GPIO_SetHSIOM(PWMUL_PORT, PWMUL_NUM, PWMUL_HSIOM);
    Cy_GPIO_SetHSIOM(PWMUH_PORT, PWMUH_NUM, PWMUH_HSIOM);
}

static inline void MCU_PhaseVEnterHighZ(void)
{
    Cy_GPIO_SetHSIOM(PWMVL_PORT, PWMVL_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_SetHSIOM(PWMVH_PORT, PWMVH_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_Clr(PWMVL_PORT, PWMVL_NUM);
    Cy_GPIO_Clr(PWMVH_PORT, PWMVH_NUM);
}

static inline void MCU_PhaseVExitHighZ(void)
{
    Cy_GPIO_SetHSIOM(PWMVL_PORT, PWMVL_NUM, PWMVL_HSIOM);
    Cy_GPIO_SetHSIOM(PWMVH_PORT, PWMVH_NUM, PWMVH_HSIOM);
}

static inline  void MCU_PhaseWEnterHighZ(void)
{
    Cy_GPIO_SetHSIOM(PWMWL_PORT, PWMWL_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_SetHSIOM(PWMWH_PORT, PWMWH_NUM, HSIOM_SEL_GPIO);
    Cy_GPIO_Clr(PWMWL_PORT, PWMWL_NUM);
    Cy_GPIO_Clr(PWMWH_PORT, PWMWH_NUM);
}

static inline void MCU_PhaseWExitHighZ(void)
{
    Cy_GPIO_SetHSIOM(PWMWL_PORT, PWMWL_NUM, PWMWL_HSIOM);
    Cy_GPIO_SetHSIOM(PWMWH_PORT, PWMWH_NUM, PWMWH_HSIOM);
}

// Temperature sensor calculations
static inline float MCU_TempSensorCalc(void)
{
    // Local result holder
    float result = 0.0f;

#if (ACTIVE_TEMP_SENSOR)  // Active IC
    // Active temperature sensor path
    // result = (adc_scale.temp_ps * SAR_read) - (TEMP_SENSOR_OFFSET / TEMP_SENSOR_SCALE)
    {
        uint16_t sar_result = (uint16_t)vres_0_motor_0_T_POWER_get_result();
        float scale       = mcu[0].adc_scale.temp_ps;
        result = (scale * sar_result) - (TEMP_SENSOR_OFFSET / TEMP_SENSOR_SCALE);
    }

#else  // Passive NTC ntcg103jf103ft1s
    // Passive NTC path using LUT
    {
		uint16_t adc_reading = (uint16_t)vres_0_motor_0_T_POWER_get_result();
    	// Check bounds
    	if (adc_reading >= ntc_adc_table[0]) 
		{
        	return ntc_temp_table[0];
    	}

    	if (adc_reading <= ntc_adc_table[NTC_TABLE_SIZE - 1]) 
		{
        	return ntc_temp_table[NTC_TABLE_SIZE - 1];
    	}
		
    	// Binary search for the right interval
    	int left = 0;
    	int right = NTC_TABLE_SIZE - 1;

    	while (right - left > 1) 
		{
        	int mid = (left + right) / 2;
        	if (ntc_adc_table[mid] < adc_reading) 
			{
            	right = mid;
        } else
			{
            	left = mid;
        	}
    	}

    	// Linear interpolation between two points
    	float adc_diff = ntc_adc_table[right] - ntc_adc_table[left];
    	float temp_diff = ntc_temp_table[right] - ntc_temp_table[left];
    	float adc_offset = adc_reading - ntc_adc_table[left];

    	result = ntc_temp_table[left] + (temp_diff * adc_offset / adc_diff);
    }
#endif

    return result;
}

static inline void MCU_InitADCs(void)
{
    //Start HPPASS Module
    pass_0_start(); 

    // ADC conversion coefficients .............................................

    // Shunt gain and scale factors
    float cs_sen = ADC_CS_CURRENT_SENSITIVITY;

    //mcu[0].adc_scale.i_uvw = (ADC_VREF_GAIN * CY_CFG_PWR_VDDA_MV * 1.0E-3f)  / ((1 << 12U) * motor[0].params_ptr->sys.analog.shunt.res * cs_gain); // [A/ticks]
	mcu[0].adc_scale.i_uvw = (ADC_VREF_GAIN * CY_CFG_PWR_VDDA_MV * 1.0E-3f) / (cs_sen * (1 << 12U)); // [A/ticks]
    mcu[0].adc_scale.v_uvw = (ADC_VREF_GAIN * CY_CFG_PWR_VDDA_MV * 1.0E-3f) / ((1 << 12U) * ADC_SCALE_VUVW); // [V/ticks]
    mcu[0].adc_scale.v_dc = (ADC_VREF_GAIN * CY_CFG_PWR_VDDA_MV * 1.0E-3f) / ((1 << 12U) * ADC_SCALE_VDC); // [V/ticks]
    mcu[0].adc_scale.v_pot = 1.0f / (1 << 12U); // [%/ticks]

#if (ACTIVE_TEMP_SENSOR)
    mcu[0].adc_scale.temp_ps =
        (ADC_VREF_GAIN * CY_CFG_PWR_VDDA_MV * 1.0E-3f)
        / ((1 << 12U) * TEMP_SENSOR_SCALE); // [Celsius/ticks]
#else
    // passive NTC
    mcu[0].adc_scale.temp_ps = 1.0f / (1 << 12U); // [1/ticks], normalized voltage wrt Vcc
#endif
}

static inline void MCU_InitTimers(void)
{
	cy_rslt_t result = CY_RSLT_SUCCESS;
    mtb_mcdi_init(&vres_0_motor_0_cfg);
    
#if defined (EXE_TIMER_ENABLED)
    //Init Execution time capture timer
    Cy_TCPWM_Counter_Init(EXE_TIMER_HW, EXE_TIMER_NUM, &EXE_TIMER_config);      
#endif

    // Clock frequencies .......................................................
    mcu[0].clk.tcpwm = Cy_SysClk_PeriPclkGetFrequency((en_clk_dst_t)CLK_TCPWM_GRP_NUM, CY_SYSCLK_DIV_8_BIT, CLK_TCPWM_NUM); // [Hz]
    mcu[0].isr0_exe.sec_per_tick = (1.0f/mcu[0].clk.tcpwm); // [sec/ticks]
    mcu[0].isr0_exe.inv_max_time = motor[0].params_ptr->sys.samp.fs0; // [1/sec]
    mcu[0].isr1_exe.sec_per_tick = (1.0f/mcu[0].clk.tcpwm); // [sec/ticks]
    mcu[0].isr1_exe.inv_max_time = motor[0].params_ptr->sys.samp.fs1; // [1/sec]A

    // Timer calculations ......................................................
    mcu[0].pwm.period = ((uint32_t)(mcu[0].clk.tcpwm * motor[0].params_ptr->sys.samp.tpwm)) & 
                       (~((uint32_t)(0x1))); // must be even

    mcu[0].pwm.duty_cycle_coeff = (float)(mcu[0].pwm.period >> 1);

    mcu[0].isr0.period = mcu[0].pwm.period * motor[0].params_ptr->sys.samp.fpwm_fs0_ratio;
    mcu[0].isr1.period = mcu[0].isr0.period * motor[0].params_ptr->sys.samp.fs0_fs1_ratio;

    // Configure timers (TCPWMs) .....................................
    uint32_t cc0 = (mcu[0].pwm.period >> 1);

    Cy_TCPWM_PWM_SetPeriod0(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_TMR_FAST].idx,
        mcu[0].isr0.period - 1U); // Sawtooth carrier

    Cy_TCPWM_PWM_SetPeriod0(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.tmr[MTB_MCDI_TMR_SYNC].idx,
        mcu[0].isr0.period - 1U); // Sawtooth carrier

    Cy_TCPWM_PWM_SetCompare0Val(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.tmr[MTB_MCDI_TMR_SYNC].idx,
        cc0); // Read ADCs at the middle of lower switches' on-times

    Cy_TCPWM_PWM_SetCompare0BufVal(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.tmr[MTB_MCDI_TMR_SYNC].idx,
        cc0); // Read ADCs at the middle of lower switches' on-times

    // U channel
    Cy_TCPWM_PWM_SetPeriod0(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_U].idx,
        mcu[0].pwm.period >> 1); // Triangle carrier

    Cy_TCPWM_PWM_SetCompare0Val(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_U].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    Cy_TCPWM_PWM_SetCompare1Val(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_U].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    Cy_TCPWM_PWM_SetCompare0BufVal(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_U].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    Cy_TCPWM_PWM_SetCompare1BufVal(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_U].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    // V channel
    Cy_TCPWM_PWM_SetPeriod0(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_V].idx,
        mcu[0].pwm.period >> 1); // Triangle carrier

    Cy_TCPWM_PWM_SetCompare0Val(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_V].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    Cy_TCPWM_PWM_SetCompare1Val(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_V].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    Cy_TCPWM_PWM_SetCompare0BufVal(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_V].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    Cy_TCPWM_PWM_SetCompare1BufVal(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_V].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    // W channel
    Cy_TCPWM_PWM_SetPeriod0(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_W].idx,
        mcu[0].pwm.period >> 1); // Triangle carrier

    Cy_TCPWM_PWM_SetCompare0Val(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_W].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    Cy_TCPWM_PWM_SetCompare1Val(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_W].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    Cy_TCPWM_PWM_SetCompare0BufVal(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_W].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    Cy_TCPWM_PWM_SetCompare1BufVal(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.pwm[MTB_MCDI_PWM_W].idx,
        mcu[0].pwm.period >> 2); // Start with duty cycle = 50%

    // Slow timer carrier
    Cy_TCPWM_PWM_SetPeriod0(
        vres_0_motor_0_cfg.tcpwmBase,
        vres_0_motor_0_cfg.tmr[MTB_MCDI_TMR_SLOW].idx,
        mcu[0].isr1.period - 1U); // Sawtooth carrier

    /* Initializes the timer used for status LED. */
    result = Cy_TCPWM_PWM_Init(PWM_LED_RED_HW, PWM_LED_RED_NUM, &PWM_LED_RED_config);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initializes the timer used for status LED. */
    result = Cy_TCPWM_PWM_Init(PWM_LED_GREEN_HW, PWM_LED_GREEN_NUM, &PWM_LED_GREEN_config);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initializes the timer used for status LED. */
    result = Cy_TCPWM_PWM_Init(PWM_LED_BLUE_HW, PWM_LED_BLUE_NUM, &PWM_LED_BLUE_config);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

	/* Enable LED PWMs now */
	Cy_TCPWM_PWM_Enable(PWM_LED_RED_HW, PWM_LED_RED_NUM);
	Cy_TCPWM_PWM_Enable(PWM_LED_GREEN_HW, PWM_LED_GREEN_NUM);
	Cy_TCPWM_PWM_Enable(PWM_LED_BLUE_HW, PWM_LED_BLUE_NUM);

    /* Starts the TCPWM for LEDs. */
    Cy_TCPWM_TriggerStart_Single(PWM_LED_RED_HW, PWM_LED_RED_NUM);
    Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_RED_HW, PWM_LED_RED_NUM, CLR_LED);
    Cy_TCPWM_TriggerStart_Single(PWM_LED_GREEN_HW, PWM_LED_GREEN_NUM);
    Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_GREEN_HW, PWM_LED_GREEN_NUM, CLR_LED);
    Cy_TCPWM_TriggerStart_Single(PWM_LED_BLUE_HW, PWM_LED_BLUE_NUM);
    Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_BLUE_HW, PWM_LED_BLUE_NUM, CLR_LED);

	/* Initializes the timer used for OCD Threshold output control. */
    result = Cy_TCPWM_PWM_Init(THPWM_PWM_HW, THPWM_PWM_NUM, &THPWM_PWM_config);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
	Cy_TCPWM_PWM_Enable(THPWM_PWM_HW, THPWM_PWM_NUM);
	
	/* Starts the TCPWM and set the OCD Threshold */
    Cy_TCPWM_TriggerStart_Single(THPWM_PWM_HW, THPWM_PWM_NUM);
    Cy_TCPWM_PWM_SetCompare0Val(THPWM_PWM_HW, THPWM_PWM_NUM, OCD_Current_to_PWM(OCD_I_AMP));

	CyDelay(50 /*Wait for output voltage to settle*/);

}

static inline void MCU_FlashInit(void)
{
    // EEPROM Emulator configuration
    mcu[0].eeprom.config.eepromSize          = srss_0_eeprom_0_SIZE;
    mcu[0].eeprom.config.simpleMode          = srss_0_eeprom_0_SIMPLEMODE;
    mcu[0].eeprom.config.wearLevelingFactor  = srss_0_eeprom_0_WEARLEVELING_FACTOR;
    mcu[0].eeprom.config.redundantCopy       = srss_0_eeprom_0_REDUNDANT_COPY;
    mcu[0].eeprom.config.blockingWrite       = srss_0_eeprom_0_BLOCKINGMODE;
    mcu[0].eeprom.config.userFlashStartAddr  = (uint32_t)&Em_Eeprom_Storage[0U];

    // Initialize EEPROM emulator
    mcu[0].eeprom.status = Cy_Em_EEPROM_Init(
        &mcu[0].eeprom.config,
        &mcu[0].eeprom.context
    );

    // Mark initialization as complete
    mcu[0].eeprom.init_done = true;
}

/******************************************************************************/
void vres_0_motor_0_fast_callback(void)
{
//Cy_GPIO_Inv(ARD_IO0_PORT, ARD_IO0_NUM);
#if defined (EXE_TIMER_ENABLED)
    MCU_StartTimeCap(&mcu[0].isr0_exe);
#endif        
    /** [SNIPPET_GET_RSLT] */
    /* For 3-shunt typically three phase currents U/V/W are being measured: */
    motor[0].sensor_iface_ptr->i_samp_0.raw =
        -mcu[0].adc_scale.i_uvw * (int16_t)vres_0_motor_0_IUP_get_result() * motor[0].params_ptr->sys.analog.shunt.current_sense_polarity;
    motor[0].sensor_iface_ptr->i_samp_1.raw =
        -mcu[0].adc_scale.i_uvw * (int16_t)vres_0_motor_0_IVP_get_result() * motor[0].params_ptr->sys.analog.shunt.current_sense_polarity;
    motor[0].sensor_iface_ptr->i_samp_2.raw =
        -mcu[0].adc_scale.i_uvw * (int16_t)vres_0_motor_0_IWP_get_result() * motor[0].params_ptr->sys.analog.shunt.current_sense_polarity;
    /** [SNIPPET_GET_RSLT] */

    STATE_MACHINE_RunISR0(&motor[0]);

    /** [SNIPPET_MOD_UPD] */
    /* Only compare0 value for the PWM symmetric mode */
    vres_0_motor_0_mod_U_set((uint16_t)(mcu[0].pwm.duty_cycle_coeff * motor[0].vars_ptr->d_uvw_cmd.u));
    vres_0_motor_0_mod_V_set((uint16_t)(mcu[0].pwm.duty_cycle_coeff * motor[0].vars_ptr->d_uvw_cmd.v));
    vres_0_motor_0_mod_W_set((uint16_t)(mcu[0].pwm.duty_cycle_coeff * motor[0].vars_ptr->d_uvw_cmd.w));

    vres_0_motor_0_mod_update();
    /** [SNIPPET_MOD_UPD] */
#if (MOTOR_CTRL_NO_OF_SCOPE_CHANNELS > 0) /*if scope is enabled*/
    ProbeScope_Sampling();
#endif

#if defined (EXE_TIMER_ENABLED)
    MCU_StopTimeCap(&mcu[0].isr0_exe);
#endif
//Cy_GPIO_Inv(ARD_IO0_PORT, ARD_IO0_NUM);
}


void vres_0_motor_0_slow_callback(void)
{
#if defined (EXE_TIMER_ENABLED)
    MCU_StartTimeCap(&mcu[0].isr1_exe);
#endif
  
    motor[0].sensor_iface_ptr->v_dc.raw = mcu[0].adc_scale.v_dc *
        (uint16_t)vres_0_motor_0_VBUS_get_result();
    motor[0].sensor_iface_ptr->pot.raw = mcu[0].adc_scale.v_pot *
        (uint16_t)vres_0_motor_0_SPEED_AN_get_result();
    motor[0].sensor_iface_ptr->temp_ps.raw = MCU_TempSensorCalc();

    /* OCD Event Check*/
	uint32_t ocd = Cy_GPIO_Read(PWR_OCD_PORT, PWR_OCD_NUM);
	if(!ocd)
	{
		ocd = Cy_GPIO_Read(PWR_OCD_PORT, PWR_OCD_NUM);

		if(!ocd)
		{
			motor[0].sensor_iface_ptr->digital.fault = !ocd;
			motor[0].faults_ptr->flags.hw.cs_ocp = motor[0].sensor_iface_ptr->digital.fault ? 0b111 : 0b000; // hw faults only cover over-current without SGD
		}
	}
    //motor[0].sensor_iface_ptr->digital.fault = !Cy_GPIO_Read(PWR_OCD_PORT, PWR_OCD_NUM);
    //motor[0].faults_ptr->flags.hw.cs_ocp = motor[0].sensor_iface_ptr->digital.fault ? 0b111 : 0b000; // hw faults only cover over-current without SGD

	/*Variable for undervoltage fault clearing */
	static _Bool uv_fault = false;
	
	/*Detect the undervoltage event first, presume that this came from 7th FET switch off*/
	if((bool)(motor[0].faults_ptr->flags_latched.sw.uv_vdc) && !uv_fault)
	{
		uv_fault = true;
	}
	/*Try to clear the undervoltage fault if requested from GUI*/
	if(!(bool)(motor[0].faults_ptr->flags_latched.sw.uv_vdc) && uv_fault)
	{
		/* Reset the power input protection*/
		Cy_GPIO_Clr(POW_EN_PORT, POW_EN_NUM);
		CyDelay(1);
    	Cy_GPIO_Set(POW_EN_PORT, POW_EN_NUM);
		uv_fault = false;
	}

    // Direction switch
#if defined(DIR_SWITCH_PORT) // hardware direction switch
    motor[0].sensor_iface_ptr->digital.dir =
        Cy_GPIO_Read(DIR_SWITCH_PORT, DIR_SWITCH_NUM);
#elif defined(N_DIR_PUSHBTN_PORT) // push-button direction control
    static bool user_btn_prev, user_btn = true;
    user_btn_prev = user_btn;
    user_btn = Cy_GPIO_Read(N_DIR_PUSHBTN_PORT, N_DIR_PUSHBTN_NUM);

    motor[0].sensor_iface_ptr->digital.dir =
        FALL_EDGE(user_btn_prev, user_btn)
            ? ~motor[0].sensor_iface_ptr->digital.dir
            : motor[0].sensor_iface_ptr->digital.dir; // toggle switch
#endif

    // Direction LED
#if defined(DIR_LED_PORT)

	if(!(bool)(motor[0].faults_ptr->flags_latched.all))
	{
		if((motor[0].vars_ptr->en) == 1)
		{
			if((motor[0].vars_ptr->dir) == 1)
			{
				Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_GREEN_HW, PWM_LED_GREEN_NUM, SET_DIM_LED);
				Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_BLUE_HW, PWM_LED_BLUE_NUM, CLR_LED);
			}
			else 
			{
				Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_GREEN_HW, PWM_LED_GREEN_NUM, CLR_LED);
				Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_BLUE_HW, PWM_LED_BLUE_NUM, SET_DIM_LED);
			}
		
		}
		else
		{
			Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_GREEN_HW, PWM_LED_GREEN_NUM, CLR_LED);
			Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_BLUE_HW, PWM_LED_BLUE_NUM, CLR_LED);
		}

	}

#endif

    // Brake switch
#if defined(N_BRK_SWITCH_PORT)
    motor[0].sensor_iface_ptr->digital.brk = !Cy_GPIO_Read(N_BRK_SWITCH_PORT, N_BRK_SWITCH_NUM);
#else
    motor[0].sensor_iface_ptr->digital.brk = 0x0; // no brake switch
#endif

    // Control ISR1
    STATE_MACHINE_RunISR1(&motor[0]);

    // SW fault LED
#if defined(N_FAULT_LED_SW_PORT) // separate LEDs for hw and sw faults
    Cy_GPIO_Write(N_FAULT_LED_SW_PORT, N_FAULT_LED_SW_NUM,
                  (bool)(!motor[0].faults_ptr->flags_latched.sw.reg));
#elif defined(FAULT_LED_ALL_PORT) // one LED for all faults

	if((bool)(motor[0].faults_ptr->flags_latched.all))
	{
		Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_RED_HW, PWM_LED_RED_NUM, FAULT_LED_LVL);
		Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_GREEN_HW, PWM_LED_GREEN_NUM, CLR_LED);
		Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_BLUE_HW, PWM_LED_BLUE_NUM, CLR_LED);
	}
	else 
	{
		Cy_TCPWM_PWM_SetCompare0Val(PWM_LED_RED_HW, PWM_LED_RED_NUM, CLR_LED);
	}

#endif

#if defined (EXE_TIMER_ENABLED)
    MCU_StopTimeCap(&mcu[0].isr1_exe);
    //Process the execution time calculation
    MCU_ProcessTimeCapISR1(&mcu[0].isr0_exe);
    MCU_ProcessTimeCapISR1(&mcu[0].isr1_exe);
#endif

}

/******************************************************************************/
void MCU_Init(uint8_t motor_id)
{
    MCU_InitChipInfo();
    MCU_InitADCs();
    MCU_InitTimers();
    
#if (MOTOR_CTRL_NO_OF_SCOPE_CHANNELS > 0) /*if scope is enabled*/
    ProbeScope_Init((uint32_t)motor[0].params_ptr->sys.samp.fs0);
#endif

    // Initial direction: positive
    motor[0].sensor_iface_ptr->digital.dir = true;
}

void MCU_EnterCriticalSection(void)
{
   mcu[0].interrupt.state = Cy_SysLib_EnterCriticalSection();
}

void MCU_ExitCriticalSection(void)
{
    Cy_SysLib_ExitCriticalSection(mcu[0].interrupt.state);
}

void MCU_GateDriverEnterHighZ(uint8_t motor_id)
{
    MCU_PhaseUEnterHighZ();
    MCU_PhaseVEnterHighZ();
    MCU_PhaseWEnterHighZ();
}

void MCU_GateDriverExitHighZ(uint8_t motor_id)
{
    MCU_PhaseUExitHighZ();
    MCU_PhaseVExitHighZ();
    MCU_PhaseWExitHighZ();
}

void MCU_StartPeripherals(uint8_t motor_id)
{
    // Enter critical section to avoid ISR interference during peripheral setup
    MCU_EnterCriticalSection();
    
#if defined (EXE_TIMER_ENABLED)
    //Start Execution time capture timer
    Cy_TCPWM_Counter_Enable(EXE_TIMER_HW, EXE_TIMER_NUM);
    Cy_TCPWM_TriggerStart_Single(EXE_TIMER_HW, EXE_TIMER_NUM);   
#endif
    
    // Enable and start peripherals for the first motor
    mtb_mcdi_enable(&vres_0_motor_0_cfg);
    mtb_mcdi_start(&vres_0_motor_0_cfg);

    // Exit critical section
    MCU_ExitCriticalSection();
}

void MCU_StopPeripherals(uint8_t motor_id)
{
    // Enter critical section to safely disable peripherals
    MCU_EnterCriticalSection();

    // Disable peripherals for the first motor
    mtb_mcdi_disable(&vres_0_motor_0_cfg);
#if defined (EXE_TIMER_ENABLED)    
    // Disable Execution time capture timer
    Cy_TCPWM_Counter_Disable(EXE_TIMER_HW, EXE_TIMER_NUM);
#endif    
    // Exit critical section
    MCU_ExitCriticalSection();
}

bool MCU_FlashRead(uint8_t motor_id, PARAMS_ID_t id, PARAMS_t* ram_data)
{
    // Ensure EEPROM is initialized
    if (!mcu[0].eeprom.init_done)
    {
        MCU_FlashInit();
    }

    // Check initialization/status
    if (CY_EM_EEPROM_SUCCESS != mcu[0].eeprom.status)
    {
        return false;
    }

    // Read the RAM data from EEPROM
    mcu[0].eeprom.status = Cy_Em_EEPROM_Read(
        0u,
        ram_data,
        sizeof(PARAMS_t),
        &mcu[0].eeprom.context
    );

    if (CY_EM_EEPROM_SUCCESS != mcu[0].eeprom.status)
    {
        return false;
    }

    // Validate the read data against the requested ID
    if (ram_data->id.code        != id.code  ||
        ram_data->id.build_config  != id.build_config ||
        ram_data->id.ver           != id.ver)
    {
        return false;
    }

    return true;
}

bool MCU_FlashWrite(uint8_t motor_id, PARAMS_t* ram_data)
{
    // Ensure EEPROM is initialized
    if (!mcu[0].eeprom.init_done)
    {
        MCU_FlashInit();
    }

    // Verify initialization/status
    if (CY_EM_EEPROM_SUCCESS != mcu[0].eeprom.status)
    {
        return false;
    }

    // Enter critical section before flash update
    MCU_EnterCriticalSection();
    
    // Write to EEPROM
    mcu[0].eeprom.status = Cy_Em_EEPROM_Write(
        0U,
        ram_data,
        sizeof(PARAMS_t),
        &mcu[0].eeprom.context
    );
    
    // Exit critical section
    MCU_ExitCriticalSection();
    
    // Check write result
    if (CY_EM_EEPROM_SUCCESS != mcu[0].eeprom.status)
    {
        return false;
    }

    return true;
}

bool MCU_ArePhaseVoltagesMeasured(uint8_t motor_id)
{
    // TODO: Implement actual check for phase voltage measurement
    return false;
}

#if defined (EXE_TIMER_ENABLED)
void MCU_StartTimeCap(MCU_TIME_CAP_t* time_cap)
{
    // Initialize start counter from hardware timer
    time_cap->start = (int32_t)(Cy_TCPWM_Counter_GetCounter(EXE_TIMER_HW, EXE_TIMER_NUM));
}

void MCU_StopTimeCap(MCU_TIME_CAP_t* time_cap)
{
    // Capture stop counter and compute raw duration in ticks
    time_cap->stop = (int32_t)(Cy_TCPWM_Counter_GetCounter(EXE_TIMER_HW, EXE_TIMER_NUM));
    time_cap->duration_ticks = time_cap->stop - time_cap->start;
}

void MCU_ProcessTimeCapISR1(MCU_TIME_CAP_t* time_cap)
{
    // Convert ticks to seconds and compute utilization
    time_cap->duration_sec = ((float)(time_cap->duration_ticks)) * (time_cap->sec_per_tick);
    time_cap->util = time_cap->duration_sec * time_cap->inv_max_time;
}

#endif