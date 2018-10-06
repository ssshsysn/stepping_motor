#include "stm32f4xx_hal.h"
#include "stepping_motor.h"

#define MOTOR_MAX       (1)             // the number of motors  
static MOTOR_INFO       motors[MOTOR_MAX] = {
    {   // Motor0 information
        {   // pin information
            {GPIOA, GPIO_PIN_10, GPIO_PIN_RESET},    // A1
            {GPIOB, GPIO_PIN_5,  GPIO_PIN_RESET},    // B1
            {GPIOA, GPIO_PIN_8,  GPIO_PIN_RESET},    // A2
            {GPIOA, GPIO_PIN_9,  GPIO_PIN_RESET},    // B2
        },
    },
};

void MotorInitialize( void )
{
    MOTOR_PIN_INFO* pMtr;
    for(uint16_t nMotor=0; nMotor < MOTOR_MAX; nMotor++ ){
        pMtr = &(motors[nMotor].phase[0]);
        HAL_GPIO_WritePin( pMtr[PHAZE_A1].port, pMtr[PHAZE_A1].pin, pMtr[PHAZE_A1].output );
        HAL_GPIO_WritePin( pMtr[PHAZE_B1].port, pMtr[PHAZE_B1].pin, pMtr[PHAZE_B1].output );
        HAL_GPIO_WritePin( pMtr[PHAZE_A2].port, pMtr[PHAZE_B1].pin, pMtr[PHAZE_A2].output );
        HAL_GPIO_WritePin( pMtr[PHAZE_B2].port, pMtr[PHAZE_B2].pin, pMtr[PHAZE_B2].output );
    }
}

void ToggleLed( void )
{
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_10);
}