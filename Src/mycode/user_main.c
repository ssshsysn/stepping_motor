#include "stm32f4xx_hal.h"
#include "interrupt_timer.h"
#include "interrupt_button.h"
#include "stepping_motor.h"

// My Initialization Code
void UserInitialize( void )
{
    MotorInitialize();
    TimerInitialize();
}

// My Main Code
void UserMain( void )
{
    button_loop();
}
