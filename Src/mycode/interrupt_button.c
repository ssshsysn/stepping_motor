#include "stm32f4xx_hal.h"
#include "main.h"
#include "interrupt_button.h"
#include "stepping_motor.h"

static uint32_t s_ButtonState = 0;
static PHASE_MODE s_PhaseMode = MTP_PHASE_FULL;

static void StartMotion(void);

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if( B1_Pin == GPIO_Pin ){
        s_ButtonState = ~s_ButtonState;
        MotorResetPosition( 0 );
        MotorSetPhaseMode( 0, s_PhaseMode );

        // 次回用
        if( MTP_PHASE_FULL == s_PhaseMode ){
            s_PhaseMode = MTP_PHASE_HALF;
        }
        else{
            s_PhaseMode = MTP_PHASE_FULL;
        }
    }
}

void button_loop(void)
{
    if( 0 == s_ButtonState ) return;

    StartMotion();
}

#define TEST_SIZE  (5)
static void StartMotion(void)
{
    static int32_t s_Index = 0;
    const static uint32_t s_pps[TEST_SIZE] = {1,2,10,50};
    const static int32_t s_pos[TEST_SIZE] = {8,16,96,496};
    if(0 != MotorIsBusy(0))  return;

    MotorMove( 0, s_pps[s_Index], s_pos[s_Index] );
    s_Index++;
    if( s_Index >= TEST_SIZE ){
        s_Index = 0;
        s_ButtonState = ~s_ButtonState;
    }
}
