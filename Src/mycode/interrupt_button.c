#include "stm32f4xx_hal.h"
#include "main.h"
#include "interrupt_button.h"
#include "stepping_motor.h"

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    //HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_10);   // TEST
}
