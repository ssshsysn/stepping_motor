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

// Phase Output States
// 2Phase   CW  : 0 -> 2 -> 4 -> 6
//          CCW : 6 -> 4 -> 2 -> 0
// 1-2Phase CW  : 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 
//          CCW : 7 -> 6 -> 5 -> 4 -> 3 -> 2 -> 1 -> 0
#define MOTOR_OFF_INDEX     (8)
#define MOTOR_PHASE_MASK    (0x00000007)
static const GPIO_PinState sc_OutpueState[MOTOR_OFF_INDEX+1][PHASE_MAX] = {
    // A1               // B1           // A2           // B2
    {GPIO_PIN_SET,      GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET    },
    {GPIO_PIN_SET,      GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET  },
    {GPIO_PIN_SET,      GPIO_PIN_SET,   GPIO_PIN_RESET, GPIO_PIN_RESET  },
    {GPIO_PIN_RESET,    GPIO_PIN_SET,   GPIO_PIN_RESET, GPIO_PIN_RESET  },
    {GPIO_PIN_RESET,    GPIO_PIN_SET,   GPIO_PIN_SET,   GPIO_PIN_RESET  },
    {GPIO_PIN_RESET,    GPIO_PIN_RESET, GPIO_PIN_SET,   GPIO_PIN_RESET  },
    {GPIO_PIN_RESET,    GPIO_PIN_RESET, GPIO_PIN_SET,   GPIO_PIN_SET    },
    {GPIO_PIN_RESET,    GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET    },
    {GPIO_PIN_RESET,    GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET  },
};

// Private functions definition 
static uint32_t MotorUpdate( MOTOR_INFO* const pMtr );
static void MotorSetup( MOTOR_INFO* const pMtr );
static void MotorOutput( const MOTOR_INFO* const pMtr );

static uint32_t MotorUpdate( MOTOR_INFO* const pMtr )
{
    pMtr->phase_index += 1 ;
    pMtr->pos++;
    pMtr->phase_index &= MOTOR_PHASE_MASK;
    return 1;
}

static void MotorSetup( MOTOR_INFO* const pMtr )
{
    if( pMtr->phase_index > MOTOR_OFF_INDEX ){
        return;
    }

    uint16_t nIndex = pMtr->phase_index;
    MOTOR_PIN_INFO* pInfo = &(pMtr->phase[0]);
    pInfo[PHASE_A1].output = sc_OutpueState[nIndex][PHASE_A1];
    pInfo[PHASE_B1].output = sc_OutpueState[nIndex][PHASE_B1];
    pInfo[PHASE_A2].output = sc_OutpueState[nIndex][PHASE_A2];
    pInfo[PHASE_B2].output = sc_OutpueState[nIndex][PHASE_B2];
}

static void MotorOutput( const MOTOR_INFO* const pMtr )
{
    const MOTOR_PIN_INFO* const pInfo = &(pMtr->phase[0]);
    HAL_GPIO_WritePin( pInfo[PHASE_A1].port, pInfo[PHASE_A1].pin, pInfo[PHASE_A1].output );
    HAL_GPIO_WritePin( pInfo[PHASE_B1].port, pInfo[PHASE_B1].pin, pInfo[PHASE_B1].output );
    HAL_GPIO_WritePin( pInfo[PHASE_A2].port, pInfo[PHASE_B1].pin, pInfo[PHASE_A2].output );
    HAL_GPIO_WritePin( pInfo[PHASE_B2].port, pInfo[PHASE_B2].pin, pInfo[PHASE_B2].output );
}

void MotorInitialize( void )
{
    MOTOR_INFO* pMtr;
    for(uint16_t nMotor=0; nMotor < MOTOR_MAX; nMotor++ ){
        motors[nMotor].phase_index   = MOTOR_OFF_INDEX;
        motors[nMotor].pos           = 0; 
        pMtr = &(motors[nMotor]);
        MotorSetup( pMtr );
        MotorOutput( pMtr );
    }
}

void MotorControl( void )
{
    MOTOR_INFO* pMtr;
    for(uint16_t nMotor=0; nMotor < MOTOR_MAX; nMotor++ ){
        pMtr = &(motors[nMotor]);
        if( 0 !=  MotorUpdate( pMtr ) ){
            MotorSetup( pMtr );
            MotorOutput( pMtr );
        }
    }
}

void ToggleLed( void )
{
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_10);
}