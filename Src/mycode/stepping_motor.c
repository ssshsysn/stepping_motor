#include "stepping_motor.h"
#include "stm32f4xx_hal.h"

extern TIM_HandleTypeDef	htim2;
static TIM_HandleTypeDef	*s_phTim = &htim2;

void Timer_Initialize( void )
{
  HAL_TIM_Base_Start_IT(s_phTim);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == s_phTim->Instance) {
    ToggleLed();
  }
}

void ToggleLed( void )
{
  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_10);
}