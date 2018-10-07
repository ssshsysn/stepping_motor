#include "stm32f4xx_hal.h"
#include "interrupt_timer.h"
#include "stepping_motor.h"

extern TIM_HandleTypeDef	htim2;
static TIM_HandleTypeDef	*s_phTim = &htim2;

void TimerInitialize( void )
{
    HAL_TIM_Base_Start_IT(s_phTim);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == s_phTim->Instance) {
        MotorControl();
    }
}
