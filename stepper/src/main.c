/*!
    \file  main.c
    \brief TIMER1 oc timebase demo
    
    \version 2019-6-5, V1.0.0, firmware for GD32VF103
*/

/*
    Copyright (c) 2019, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this 
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice, 
       this list of conditions and the following disclaimer in the documentation 
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors 
       may be used to endorse or promote products derived from this software without 
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT 
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include "gd32v_pjt_include.h"
#include "systick.h"

#define MOTOR_1 1
#define MOTOR_2 1
#define MOTOR_3 1
#define MOTOR_4 1

/**
    \brief      configure the GPIO ports
    \param[in]  none
    \param[out] none
    \retval     none
  */
static void gpio_config(void)
{
    /* Configure led GPIO port */ 
    gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_13);
}

static void rcu_config(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
#if MOTOR_1
    rcu_periph_clock_enable(RCU_TIMER1);
#endif
#if MOTOR_2
    rcu_periph_clock_enable(RCU_TIMER2);
#endif
#if MOTOR_3
    rcu_periph_clock_enable(RCU_TIMER3);
#endif
#if MOTOR_4
    rcu_periph_clock_enable(RCU_TIMER4);
#endif
}

#define BOP_SET_BIT(X) (X)
#define BOP_CLR_BIT(X) (X << 16)

typedef struct 
{
    uint32_t steps_per_revolution;
    uint32_t timer;
    uint32_t timer_channel;
    uint32_t gpio_periph;
    uint32_t pin1, pin2, pin3, pin4;
} motor_config;

typedef struct
{
    uint32_t timer;
    uint32_t timer_channel;
    uint32_t gpio_periph;
    uint32_t volatile steps_left;
    uint32_t volatile step_number;
    uint32_t steps_per_revolution;
    uint32_t direction;
    uint32_t steps[8];
    uint32_t standby;
} motor_runtime;

/**
    \brief      configure the TIMER peripheral
    \param[in]  none
    \param[out] none
    \retval     none
  */
void motor_configure(motor_runtime *cfg_out, motor_config const *cfg_in,
             int32_t steps_to_move, uint32_t rpm)
{
    cfg_out->timer = cfg_in->timer;
    cfg_out->timer_channel = cfg_in->timer_channel;
    cfg_out->gpio_periph = cfg_in->gpio_periph;

    cfg_out->steps_left = abs(steps_to_move);
    cfg_out->direction = steps_to_move > 0;
    cfg_out->step_number = 0;
    cfg_out->steps_per_revolution = cfg_in->steps_per_revolution;

    cfg_out->steps[0] = 
        BOP_CLR_BIT(cfg_in->pin1) | BOP_CLR_BIT(cfg_in->pin2) | BOP_CLR_BIT(cfg_in->pin3) | BOP_SET_BIT(cfg_in->pin4); // 0001
    cfg_out->steps[1] = 
        BOP_CLR_BIT(cfg_in->pin1) | BOP_CLR_BIT(cfg_in->pin2) | BOP_SET_BIT(cfg_in->pin3) | BOP_SET_BIT(cfg_in->pin4); // 0011
    cfg_out->steps[2] = 
        BOP_CLR_BIT(cfg_in->pin1) | BOP_CLR_BIT(cfg_in->pin2) | BOP_SET_BIT(cfg_in->pin3) | BOP_CLR_BIT(cfg_in->pin4); // 0010
    cfg_out->steps[3] = 
        BOP_CLR_BIT(cfg_in->pin1) | BOP_SET_BIT(cfg_in->pin2) | BOP_SET_BIT(cfg_in->pin3) | BOP_CLR_BIT(cfg_in->pin4); // 0110

    cfg_out->steps[4] = 
        BOP_CLR_BIT(cfg_in->pin1) | BOP_SET_BIT(cfg_in->pin2) | BOP_CLR_BIT(cfg_in->pin3) | BOP_CLR_BIT(cfg_in->pin4); // 0100
    cfg_out->steps[5] = 
        BOP_SET_BIT(cfg_in->pin1) | BOP_SET_BIT(cfg_in->pin2) | BOP_CLR_BIT(cfg_in->pin3) | BOP_CLR_BIT(cfg_in->pin4); // 1100
    cfg_out->steps[6] = 
        BOP_SET_BIT(cfg_in->pin1) | BOP_CLR_BIT(cfg_in->pin2) | BOP_CLR_BIT(cfg_in->pin3) | BOP_CLR_BIT(cfg_in->pin4); // 1000
    cfg_out->steps[7] = 
        BOP_SET_BIT(cfg_in->pin1) | BOP_CLR_BIT(cfg_in->pin2) | BOP_CLR_BIT(cfg_in->pin3) | BOP_SET_BIT(cfg_in->pin4);  // 1001

    cfg_out->standby =
        BOP_CLR_BIT(cfg_in->pin1) | BOP_CLR_BIT(cfg_in->pin2) | BOP_CLR_BIT(cfg_in->pin3) | BOP_CLR_BIT(cfg_in->pin4);

    gpio_init(cfg_out->gpio_periph, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ,
        cfg_in->pin1 | cfg_in->pin2 | cfg_in->pin3 | cfg_in->pin4);

    GPIO_BOP(cfg_out->gpio_periph) = cfg_out->standby;

    uint32_t step_delay_us = 60 * 1000 * 1000 / cfg_out->steps_per_revolution / rpm;

    /* ----------------------------------------------------------------------------
    Timer Configuration: 
    Timer CLK = SystemCoreClock/108 = 1MHz.
    Timer configuration is timing mode, and the timing is how many microseconds
    it takes for a motor step.
    ---------------------------------------------------------------------------- */
    timer_oc_parameter_struct timer_ocinitpara;
    timer_parameter_struct timer_initpara;

    timer_deinit(cfg_out->timer);
    /* initialize TIMER init parameter struct */
    timer_struct_para_init(&timer_initpara);
    /* TIMER1 configuration */
    timer_initpara.prescaler         = (uint16_t) (SystemCoreClock / 1000000U);
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = step_delay_us;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_init(cfg_out->timer, &timer_initpara);

    /* initialize TIMER channel output parameter struct */
    timer_channel_output_struct_para_init(&timer_ocinitpara);
    /* CH0,CH1 and CH2 configuration in OC timing mode */
    timer_ocinitpara.outputstate  = TIMER_CCX_ENABLE;
    timer_ocinitpara.ocpolarity   = TIMER_OC_POLARITY_HIGH;
    timer_ocinitpara.ocidlestate  = TIMER_OC_IDLE_STATE_LOW;
    timer_channel_output_config(cfg_out->timer, cfg_out->timer_channel, &timer_ocinitpara);

    /* CH0 configuration in OC timing mode */
    timer_channel_output_pulse_value_config(cfg_out->timer, cfg_out->timer_channel, 0);
    timer_channel_output_mode_config(cfg_out->timer, cfg_out->timer_channel, TIMER_OC_MODE_TIMING);
    timer_channel_output_shadow_config(cfg_out->timer, cfg_out->timer_channel, TIMER_OC_SHADOW_DISABLE);

    timer_interrupt_enable(cfg_out->timer, cfg_out->timer_channel);

    timer_enable(cfg_out->timer);
}

#if MOTOR_1
static motor_runtime motor1;
#endif
#if MOTOR_2
static motor_runtime motor2;
#endif
#if MOTOR_3
static motor_runtime motor3;
#endif
#if MOTOR_4
static motor_runtime motor4;
#endif

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    /* peripheral clock enable */
    rcu_config();

    /* GPIO configure */
    gpio_config();

    // longan_led_init();
    LEDR(1);
    LEDG(1);
    LEDB(1);

    eclic_global_interrupt_enable();
    eclic_set_nlbits(ECLIC_GROUP_LEVEL3_PRIO1);
#if MOTOR_1
    eclic_irq_enable(TIMER1_IRQn, 1, 0);
#endif
#if MOTOR_2
    eclic_irq_enable(TIMER2_IRQn, 2, 0);
#endif
#if MOTOR_3
    eclic_irq_enable(TIMER3_IRQn, 3, 0);
#endif
#if MOTOR_4
    eclic_irq_enable(TIMER4_IRQn, 4, 0);
#endif

#if MOTOR_1
    static motor_config const config1 = {
        .steps_per_revolution = 4096,
        .timer = TIMER1,
        .timer_channel = TIMER_INT_CH0,
        .gpio_periph = GPIOA,
        .pin1 = GPIO_PIN_0,
        .pin2 = GPIO_PIN_1,
        .pin3 = GPIO_PIN_2,
        .pin4 = GPIO_PIN_3
    };
#endif

#if MOTOR_2
    static motor_config const config2 = {
        .steps_per_revolution = 4096,
        .timer = TIMER2,
        .timer_channel = TIMER_INT_CH0,
        .gpio_periph = GPIOB,
        .pin1 = GPIO_PIN_6,
        .pin2 = GPIO_PIN_7,
        .pin3 = GPIO_PIN_8,
        .pin4 = GPIO_PIN_9
    };
#endif

#if MOTOR_3
    static motor_config const config3 = {
        .steps_per_revolution = 4096,
        .timer = TIMER3,
        .timer_channel = TIMER_INT_CH0,
        .gpio_periph = GPIOB,
        .pin1 = GPIO_PIN_0,
        .pin2 = GPIO_PIN_1,
        .pin3 = GPIO_PIN_10,
        .pin4 = GPIO_PIN_11
    };
#endif

#if MOTOR_4
    static motor_config const config4 = {
        .steps_per_revolution = 4096,
        .timer = TIMER4,
        .timer_channel = TIMER_INT_CH0,
        .gpio_periph = GPIOB,
        .pin1 = GPIO_PIN_12,
        .pin2 = GPIO_PIN_13,
        .pin3 = GPIO_PIN_14,
        .pin4 = GPIO_PIN_15
    };
#endif

    static int32_t steps = 2048;

    while (1)
    {
        LEDR(1);

#if MOTOR_1
        motor_configure(&motor1, &config1, steps, 10U);
#endif
#if MOTOR_2
        motor_configure(&motor2, &config2, steps, 5U);
#endif
#if MOTOR_3
        motor_configure(&motor3, &config3, steps, 6U);
#endif
#if MOTOR_4
        motor_configure(&motor4, &config4, steps, 7U);
#endif

#if MOTOR_1
        while (motor1.steps_left > 0);
#endif
#if MOTOR_2
        while (motor2.steps_left > 0);
#endif
#if MOTOR_3
        while (motor3.steps_left > 0);
#endif
#if MOTOR_4
        while (motor4.steps_left > 0);
#endif

        LEDR(0);
        delay_1ms(1000);
        steps *= -1;
    }
}

static
void process_motor_intr(motor_runtime * const mrt)
{
    bool const ch_intr = SET == timer_interrupt_flag_get(mrt->timer, mrt->timer_channel);

    if (ch_intr) {
        /* clear channel 0 interrupt bit */
        timer_interrupt_flag_clear(mrt->timer, mrt->timer_channel);

        if (mrt->steps_left == 0)
        {
            GPIO_BOP(mrt->gpio_periph) = mrt->standby;
            timer_interrupt_disable(mrt->timer, mrt->timer_channel);
        }
        else
        {
            if (mrt->direction == 1)
            {
                mrt->step_number += 1;
                if (mrt->step_number == mrt->steps_per_revolution)
                {
                    mrt->step_number = 0;
                }
            }
            else
            {
                if (mrt->step_number == 0)
                {
                    mrt->step_number = mrt->steps_per_revolution;
                }
                mrt->step_number -= 1;
            }

            uint32_t const idx = mrt->step_number % (sizeof(mrt->steps) / sizeof(mrt->steps[0]));
            GPIO_BOP(mrt->gpio_periph) = mrt->steps[idx];
            mrt->steps_left -= 1;
        }
    }
}

#if MOTOR_1
void TIMER1_IRQHandler(void)
{
    process_motor_intr(&motor1);
}
#endif

#if MOTOR_2
void TIMER2_IRQHandler(void)
{
    process_motor_intr(&motor2);
}
#endif

#if MOTOR_3
void TIMER3_IRQHandler(void)
{
    process_motor_intr(&motor3);
}
#endif

#if MOTOR_4
void TIMER4_IRQHandler(void)
{
    process_motor_intr(&motor4);
}
#endif
