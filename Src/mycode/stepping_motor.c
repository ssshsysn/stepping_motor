#include "stm32f4xx_hal.h"
#include "stepping_motor.h"

// Motor Status
typedef enum {
    MTS_IDLE        = 0,    // IDLE
    MTS_RUN_ACCEL,          // RUNNING ACCEL(Slow Up)
    MTS_RUN_CONST,          // RUNNING CONSTANT
    MTS_RUN_DECEL,          // RUNNING DECEL(Slow Down)
    MTS_BREAK,              // BREAKING(Have Timeout)
}MOTOR_STATUS;

// Motor Direction
typedef enum {
    MTD_CW      = 0,
    MTD_CCW,
}MOTOR_DIRECTION;

// Phase
#define PHASE_A1    (0)
#define PHASE_B1    (1)
#define PHASE_A2    (2)
#define PHASE_B2    (3)
#define PHASE_MAX   (4)
// Phase control index informations
#define MOTOR_OFF_INDEX     (8)
#define MOTOR_PHASE_MASK    (0x00000007)
// Phase Output States
// 2Phase   CW  : 0 -> 2 -> 4 -> 6
//          CCW : 6 -> 4 -> 2 -> 0
// 1-2Phase CW  : 0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 
//          CCW : 7 -> 6 -> 5 -> 4 -> 3 -> 2 -> 1 -> 0
static const GPIO_PinState sc_OutputState[MOTOR_OFF_INDEX+1][PHASE_MAX] = {
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

// Phase mode
typedef enum {
    MTP_PHASE_2     = 0,    // 2Phase mode
    MTP_PHASE_1_2,          // 1-2Phase mode
}PHASE_MODE;

// Motor pin information structure
typedef struct {
    GPIO_TypeDef*       port;           // GPIO PORT NUMBER
    uint16_t            pin;            // GPIO PIN NUMBER
    GPIO_PinState       output;         // PIN STATE(GPIO_PIN_SET,GPIO_PIN_RESET)         
}MOTOR_PIN_INFO;

// Motor information structure
typedef struct {
    MOTOR_STATUS        status;                 // motor status
    MOTOR_DIRECTION     direction;              // motor direction
    PHASE_MODE          phase_mode;             // phase mode
    uint32_t            phase_index;            // phase current index
    uint32_t            phase_pos;              // phase current position
    MOTOR_PIN_INFO      phase[PHASE_MAX];       // phase information
    uint32_t            break_timeout;          // timeout for motor failure mode
    int32_t             pos;                    // motor position                    
}MOTOR_INFO;

// Motor information
#define MOTOR_MAX       (1)             // the number of motors  
static MOTOR_INFO       motors[MOTOR_MAX] = {
    {   // Motor0 information
        MTS_IDLE,       // motor status
        MTD_CW,         // motor cw
        MTP_PHASE_2,    // phase mode
        MOTOR_OFF_INDEX,// phase current index
        0,              // phase current position
        {               // phase(pin) information
            {GPIOA, GPIO_PIN_10, GPIO_PIN_RESET},    // A1
            {GPIOB, GPIO_PIN_5,  GPIO_PIN_RESET},    // B1
            {GPIOA, GPIO_PIN_8,  GPIO_PIN_RESET},    // A2
            {GPIOA, GPIO_PIN_9,  GPIO_PIN_RESET},    // B2
        },
        0,              // breaking timeout
        0,              // motor position
    },
};

// Private functions definition 
static uint32_t MotorUpdate( MOTOR_INFO* const pMtr );
static void MotorUpdateNextStatus( MOTOR_INFO* const pMtr );
static void MotorUpdatePhase( MOTOR_INFO* const pMtr );
static void MotorUpdateCurrentPosition( MOTOR_INFO* const pMtr );
static void MotorUpdateBreakingTimeout( MOTOR_INFO* const pMtr );
static void MotorSetup( MOTOR_INFO* const pMtr );
static void MotorOutput( const MOTOR_INFO* const pMtr );

// function : Update for Motor information
static uint32_t MotorUpdate( MOTOR_INFO* const pMtr )
{
    // Check Status
    if( pMtr->status == MTS_IDLE )  return 0;
    // Update Breaking Timeout
    MotorUpdateBreakingTimeout( pMtr );
    // Update Phase Index
    MotorUpdatePhase( pMtr );
    // Update Current Motor position
    MotorUpdateCurrentPosition( pMtr );
    // Update status for next process
    MotorUpdateNextStatus( pMtr );
    return 1;
}

// function : Update for motor Next status
static void MotorUpdateNextStatus( MOTOR_INFO* const pMtr )
{
    switch( pMtr->status ){
        default:
        case MTS_IDLE:
        case MTS_RUN_ACCEL:
        case MTS_RUN_CONST:
        case MTS_RUN_DECEL:
            break;
        case MTS_BREAK:
            if( pMtr->break_timeout == 0 ) pMtr->status = MTS_IDLE;
            break;
    }
}

// function : Update for Phase Index
static void MotorUpdatePhase( MOTOR_INFO* const pMtr )
{
    // Phase Index
    // Check Breaking timeout
    if( pMtr->status == MTS_BREAK ){
        // break_timeout = 0 then output off and chahge status to IDLE
        // break_timeout > 0 then output keep
        if( pMtr->break_timeout == 0 ){
            pMtr->phase_pos     = pMtr->phase_index;
            pMtr->phase_index   = MOTOR_OFF_INDEX;
        }
        return;
    }

    // Update Phase Index
    int32_t direction_update;
    // Phase mode
    if( pMtr->phase_mode == MTP_PHASE_2 )   direction_update = 2;   // 2Phase mode
    else                                    direction_update = 1;   // 1-2Phase mode
    // Direction
    if( pMtr->direction == MTD_CCW )        direction_update *= -1;
    // Update
    pMtr->phase_index += direction_update;
    pMtr->phase_index &= MOTOR_PHASE_MASK;
}

// function : Update for Current Position
static void MotorUpdateCurrentPosition( MOTOR_INFO* const pMtr )
{
    if( pMtr->status == MTS_BREAK || pMtr->status == MTS_IDLE ) return;
    if( pMtr->phase_mode == MTP_PHASE_1_2 ){
        // now Halfstep position then return
        if( pMtr->phase_pos%2 ) return;
    }
    // Update
    if( pMtr->direction == MTD_CW ) pMtr->pos++;
    else                            pMtr->pos--;
}

// function : Update for Breaking Timeout
static void MotorUpdateBreakingTimeout( MOTOR_INFO* const pMtr )
{
    if( pMtr->status != MTS_BREAK ) return;
    if( pMtr->break_timeout > 0 )   pMtr->break_timeout--;
}

// function : Set up for Motor output status
static void MotorSetup( MOTOR_INFO* const pMtr )
{
    // check pahse index range
    if( pMtr->phase_index > MOTOR_OFF_INDEX )   return;

    uint16_t nIndex = pMtr->phase_index;
    MOTOR_PIN_INFO* pInfo = &(pMtr->phase[0]);
    pInfo[PHASE_A1].output = sc_OutputState[nIndex][PHASE_A1];
    pInfo[PHASE_B1].output = sc_OutputState[nIndex][PHASE_B1];
    pInfo[PHASE_A2].output = sc_OutputState[nIndex][PHASE_A2];
    pInfo[PHASE_B2].output = sc_OutputState[nIndex][PHASE_B2];
}

// function : Output pins
static void MotorOutput( const MOTOR_INFO* const pMtr )
{
    const MOTOR_PIN_INFO* const pInfo = &(pMtr->phase[0]);
    HAL_GPIO_WritePin( pInfo[PHASE_A1].port, pInfo[PHASE_A1].pin, pInfo[PHASE_A1].output );
    HAL_GPIO_WritePin( pInfo[PHASE_B1].port, pInfo[PHASE_B1].pin, pInfo[PHASE_B1].output );
    HAL_GPIO_WritePin( pInfo[PHASE_A2].port, pInfo[PHASE_B1].pin, pInfo[PHASE_A2].output );
    HAL_GPIO_WritePin( pInfo[PHASE_B2].port, pInfo[PHASE_B2].pin, pInfo[PHASE_B2].output );
}

// function : Initialize for Motor information
void MotorInitialize( void )
{
    MOTOR_INFO* pMtr;
    for(uint16_t nMotor=0; nMotor < MOTOR_MAX; nMotor++ ){
        motors[nMotor].status        = MTS_IDLE;
        motors[nMotor].direction     = MTD_CW;
        motors[nMotor].phase_mode    = MTP_PHASE_2;
        motors[nMotor].phase_index   = MOTOR_OFF_INDEX;
        motors[nMotor].phase_pos     = 0;
        motors[nMotor].break_timeout = 0,
        motors[nMotor].pos           = 0; 
        pMtr = &(motors[nMotor]);
        // Output Initial Position
        motors[nMotor].phase_index   = 0;
        MotorSetup( pMtr );
        MotorOutput( pMtr );
        // Output off
        motors[nMotor].phase_index   = MOTOR_OFF_INDEX;
        motors[nMotor].phase_pos     = 0;
        MotorSetup( pMtr );
        MotorOutput( pMtr );
        // TEST
        motors[nMotor].status        = MTS_BREAK;
        motors[nMotor].direction     = MTD_CCW;
        motors[nMotor].phase_index   = 0;
        motors[nMotor].break_timeout = 6;   
    }
}

// function : Control for Motor output status
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

// function : Toggle LED for test
void ToggleLed( void )
{
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_10);
}