/*******************************************************************************
* File Name     : main.c
*
* Description   : This source file contains the main routine for non-secure
*                 application in the CM33 CPU. it demonstrates the generation
*                 of asymmetric PWM signals using two compare/capture registers.
*
* Related Document : See README.md
*
*******************************************************************************
* Copyright 2023-2025, Cypress Semiconductor Corporation (an Infineon company) or
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

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "retarget_io_init.h"

/*******************************************************************************
 * Macros
 *******************************************************************************/
#define UART_IRQ_PRIORITY           (3U)
#define COMPARE_VALUE_DELTA         (100U)
#define DELAY_BETWEEN_READ_MS       (100U)
#define FULL_DUTY_CYCLE_PERCENT     (100.0F)

/* The timeout value in microsecond used to wait for core to be booted */
#define CM55_BOOT_WAIT_TIME_USEC    (10U)

#define COMPARE_VAL_MIN             (0)
#define COMPARE_AVG_DIVIDER         (2U)
/* App boot address for CM55 project */
#define CM55_APP_BOOT_ADDR          (CYMEM_CM33_0_m55_nvm_START + \
                                        CYBSP_MCUBOOT_HEADER_SIZE)

/*******************************************************************************
* Global Variables
*******************************************************************************/
uint32_t period = 0; /* Variable to store period value of TCPWM block */
int32_t compare0_value = -1; /* Variable to store the CC0 value of TCPWM block */
int32_t compare1_value = -1; /* Variable to store the CC1 value of TCPWM block */
float duty_cycle = 0.0f; /* duty cycle calculated from compare values */
volatile bool uart_read_flag = false;

/*******************************************************************************
* Function definitions
*******************************************************************************/
/*******************************************************************************
* Function Name: print_instructions
********************************************************************************
* Summary:
* Prints set of instructions.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void print_instructions(void)
{
    printf("=================================================================\r\n"
            "Instructions:\r\n"
            "=================================================================\r\n"
            "Press 'w' : To increase the duty cycle\r\n"
            "Press 's' : To decrease the duty cycle\r\n"
            "Press 'a' : To shift waveform towards left\r\n"
            "Press 'd' : To shift waveform towards right\r\n"
            "=================================================================\r\n");
}

/*******************************************************************************
* Function Name: uart_event_handler
********************************************************************************
* Summary:
* UART event handler callback function. Sets the read flag to true upon
* successful reception of data.
*
* Parameters:
*  handler_arg - argument for the handler provided during callback registration
*  event - interrupt cause flags
*
* Return:
*  void
*
*******************************************************************************/
static void uart_event_handler(uint32_t event)
{
    if (CY_SCB_UART_RECEIVE_DONE_EVENT == (event & CY_SCB_UART_RECEIVE_DONE_EVENT))
    {
        /* Set read flag */
        uart_read_flag = true;
    }
    else if (CY_SCB_UART_RECEIVE_ERR_EVENT == event)
    {
        printf("UART: Receive Error!\n");
        handle_app_error();
    }
    else
    {
        printf("UART: Unknown Error vent: %u\n", (unsigned int)event);
        handle_app_error();
    }
}
 
/*******************************************************************************
* Function Name: uart_isr
********************************************************************************
* Summary:
*  This function is registered to be called when UART interrupt occurs.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
static void uart_isr(void)
{
    /* Just call PDL UART intr handler */
    Cy_SCB_UART_Interrupt(CYBSP_DEBUG_UART_HW, &DEBUG_UART_context);
}

/*******************************************************************************
* Function Name: process_key_press
********************************************************************************
* Summary:
* Function to process the key pressed. Depending on the command passed as
* parameter, new compare values are calculated. The values are written to
* the respective buffer registers and a compare swap is issued.
*
* Parameters:
*  key_pressed - command read through terminal
*
* Return:
*  void
*
*******************************************************************************/
static void process_key_press(char key_pressed)
{
    printf("Pressed %c key\r\n", key_pressed);

    switch(key_pressed)
    {
        /* Increase duty cycle */
        case 's':
            compare0_value += COMPARE_VALUE_DELTA;
            compare1_value += COMPARE_VALUE_DELTA;
            if( compare0_value > period )
                compare0_value = period;
            if( compare1_value > period )
                compare1_value = period;
            break;
        /* Decrease duty cycle */
        case 'w':
            compare0_value -= COMPARE_VALUE_DELTA;
            compare1_value -= COMPARE_VALUE_DELTA;
            if( compare0_value < COMPARE_VAL_MIN )
                compare0_value = COMPARE_VAL_MIN;
            if( compare1_value < COMPARE_VAL_MIN )
                compare1_value = COMPARE_VAL_MIN;
            break;
        /* Shift waveform to left */
        case 'a':
            compare0_value -= COMPARE_VALUE_DELTA;
            compare1_value += COMPARE_VALUE_DELTA;
            if( compare0_value < COMPARE_VAL_MIN )
                compare0_value = COMPARE_VAL_MIN;
            if( compare1_value > period )
                compare1_value = period;
            break;
        /* Shift waveform to right */
        case 'd':
            compare0_value += COMPARE_VALUE_DELTA;
            compare1_value -= COMPARE_VALUE_DELTA;
            if( compare0_value > period )
                compare0_value = period;
            if( compare1_value < COMPARE_VAL_MIN )
                compare1_value = COMPARE_VAL_MIN;
            break;
        default:
            printf("\r\n Wrong key pressed !! See below instructions:\r\n");
            print_instructions();
        return;
    }

    duty_cycle = (((float)period - ((compare0_value + compare1_value)
            /COMPARE_AVG_DIVIDER)) / (float)period) * FULL_DUTY_CYCLE_PERCENT;

    printf("Period:%lu\t|Compare0:%ld\t|Compare1:%ld\t|Duty Cycle: %.1f\r\n",
            (unsigned long) period, (long) compare0_value,
            (long) compare1_value, (double) duty_cycle);

    /* Set new values for CC0/1 compare buffers */
    Cy_TCPWM_PWM_SetCompare0BufVal(CYBSP_TCPWM_0_GRP_1_PWM_0_HW,
            CYBSP_TCPWM_0_GRP_1_PWM_0_NUM,
            compare0_value);
    Cy_TCPWM_PWM_SetCompare1BufVal(CYBSP_TCPWM_0_GRP_1_PWM_0_HW,
            CYBSP_TCPWM_0_GRP_1_PWM_0_NUM,
            compare1_value);

    /* Trigger compare swap with its buffer values */
    Cy_TCPWM_TriggerCaptureOrSwap_Single(CYBSP_TCPWM_0_GRP_1_PWM_0_HW,
            CYBSP_TCPWM_0_GRP_1_PWM_0_NUM);
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function of the CM33 non-secure application.
* It Initializes the retarget-IO and sets up a uart_event_handler function 
* callback to be triggered upon receiving data. It sets up the TCPWM in 
* PWM mode. The infinite loop sets up asynchronous UART read and depending 
* on the command read, the compare values are modified to change
* the duty cycle and phase.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    /* Variable to store the read command through UART */
    char uart_read_value;

    cy_stc_sysint_t uart_scb_irq_cfg=
    {
        .intrSrc      = CYBSP_DEBUG_UART_IRQ,
        .intrPriority = UART_IRQ_PRIORITY,
    };

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io middleware */
    init_retarget_io();

    Cy_SCB_SetRxInterruptMask(CYBSP_DEBUG_UART_HW, CY_SCB_UART_RX_TRIGGER);

    /* UART base and context referenced from retarget-IO init funciton. */
    Cy_SCB_UART_RegisterCallback(CYBSP_DEBUG_UART_HW,
            (cy_cb_scb_uart_handle_events_t)uart_event_handler,
            &DEBUG_UART_context);
    Cy_SysInt_Init(&uart_scb_irq_cfg, uart_isr);
    NVIC_EnableIRQ(uart_scb_irq_cfg.intrSrc);

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("********* PSOC Edge MCU: TCPWM in PWM Mode with Dual Compare Capture *********\r\n");

    /* Initialize and enable the TCPWM blocks */
    Cy_TCPWM_PWM_Init(CYBSP_TCPWM_0_GRP_1_PWM_0_HW, CYBSP_TCPWM_0_GRP_1_PWM_0_NUM,
            &CYBSP_TCPWM_0_GRP_1_PWM_0_config);
    Cy_TCPWM_PWM_Init(CYBSP_TCPWM_0_GRP_1_PWM_1_HW, CYBSP_TCPWM_0_GRP_1_PWM_1_NUM,
            &CYBSP_TCPWM_0_GRP_1_PWM_1_config);

    Cy_TCPWM_PWM_Enable(CYBSP_TCPWM_0_GRP_1_PWM_0_HW, CYBSP_TCPWM_0_GRP_1_PWM_0_NUM);
    Cy_TCPWM_PWM_Enable(CYBSP_TCPWM_0_GRP_1_PWM_1_HW, CYBSP_TCPWM_0_GRP_1_PWM_1_NUM);

    /* Fetch the initial values of period, CC0 and CC1 registers configured
     * through the design file 
     */
    period = Cy_TCPWM_PWM_GetPeriod0(CYBSP_TCPWM_0_GRP_1_PWM_0_HW, CYBSP_TCPWM_0_GRP_1_PWM_0_NUM);
    compare1_value = Cy_TCPWM_PWM_GetCompare1Val(CYBSP_TCPWM_0_GRP_1_PWM_0_HW,
            CYBSP_TCPWM_0_GRP_1_PWM_0_NUM);
    compare0_value = Cy_TCPWM_PWM_GetCompare0Val(CYBSP_TCPWM_0_GRP_1_PWM_0_HW,
            CYBSP_TCPWM_0_GRP_1_PWM_0_NUM);

    /* Start the TCPWM blocks */
    Cy_TCPWM_TriggerStart_Single(CYBSP_TCPWM_0_GRP_1_PWM_0_HW, CYBSP_TCPWM_0_GRP_1_PWM_0_NUM);
    Cy_TCPWM_TriggerStart_Single(CYBSP_TCPWM_0_GRP_1_PWM_1_HW, CYBSP_TCPWM_0_GRP_1_PWM_1_NUM);

    print_instructions();

    /* Enable CM55. CM55_APP_BOOT_ADDR must be updated if CM55 memory layout is changed. */
    Cy_SysEnableCM55(MXCM55, CM55_APP_BOOT_ADDR, CM55_BOOT_WAIT_TIME_USEC);

    for (;;)
    {
        Cy_SCB_UART_Receive(CYBSP_DEBUG_UART_HW,(void*) &uart_read_value,
                sizeof(uart_read_value), &DEBUG_UART_context);

        /* Check if the read flag has been set by the callback */
        if(uart_read_flag)
        {
            /* Clear read flag */
            uart_read_flag = false;

            /* Process the command and modify the compare values to change the
             * duty cycle and phase.
             */
            process_key_press(uart_read_value);
        }

        /* Delay between next read */
        Cy_SysLib_Delay(DELAY_BETWEEN_READ_MS);
    }
}

/* [] END OF FILE */

