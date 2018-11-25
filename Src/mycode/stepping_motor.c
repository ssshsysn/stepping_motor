#include "stm32f4xx_hal.h"
#include "stepping_motor.h"

// Interrupt Timer interval
#define INTERRUPT_TIMER_INTERVAL  (1000)    // 1000ms / Interrupt interval(ms)

// default value for PPS
#define DEFAULT_PPS               (1000)

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
    MTP_PHASE_FULL     = 0,  // FULL-STEP Phase mode
    MTP_PHASE_HALF,          // HALF-STEP Phase mode
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
    uint32_t            pps;                    // PPS
    uint32_t            pps_timer;              // PPS count down timer for phase output
    PHASE_MODE          phase_mode;             // phase mode
    int32_t             phase_index_update_num; // phase index update number
    uint32_t            phase_index;            // phase current index
    uint32_t            phase_pos;              // phase current position
    MOTOR_PIN_INFO      phase[PHASE_MAX];       // phase information
    uint32_t            break_timeout;          // breaking timeout
    uint32_t            break_timer;            // count down for breaking timeout 
    int32_t             motor_position;         // motor position
    int32_t             target_position;        // target position
}MOTOR_INFO;

// Motor information
#define MOTOR_MAX       (1)             // the number of motors  
static MOTOR_INFO       motors[MOTOR_MAX] = {
    {   // Motor0 information
        MTS_IDLE,       // motor status
        MTD_CW,         // motor cw
        DEFAULT_PPS,    // PPS
        (uint32_t)(INTERRUPT_TIMER_INTERVAL/DEFAULT_PPS),      // PPS count down timer for phase output
        MTP_PHASE_FULL, // phase mode
        2,              // phase index update number
        MOTOR_OFF_INDEX,// phase current index
        0,              // phase current position
        {               // phase(pin) information
            {GPIOA, GPIO_PIN_10, GPIO_PIN_RESET},    // A1
            {GPIOB, GPIO_PIN_5,  GPIO_PIN_RESET},    // B1
            {GPIOA, GPIO_PIN_8,  GPIO_PIN_RESET},    // A2
            {GPIOA, GPIO_PIN_9,  GPIO_PIN_RESET},    // B2
        },
        10,             // breaking timeout
        0,              // count down for breaking timeout 
        0,              // motor position
        0,              // target position
    },
};

// Private functions definition 
static void MotorUpdate( MOTOR_INFO* const pMtr );
static void MotorUpdateNextStatus( MOTOR_INFO* const pMtr );
static uint32_t MotorUpdatePhase( MOTOR_INFO* const pMtr );
static uint32_t MotorUpdatePhaseIfConst( MOTOR_INFO* const pMtr );
static uint32_t MotorUpdatePhaseIfBreak( MOTOR_INFO* const pMtr );
static void MotorUpdateCurrentPosition( MOTOR_INFO* const pMtr );
static void MotorUpdatePPSTimer( MOTOR_INFO* const pMtr );
static void MotorDecisionPhaseIndexUpdateNumber( MOTOR_INFO* const pMtr );
static void MotorSetup( MOTOR_INFO* const pMtr );
static void MotorOutput( const MOTOR_INFO* const pMtr );

// function : Update for Motor information
static void MotorUpdate( MOTOR_INFO* const pMtr )
{
    // Check Status
    if( pMtr->status == MTS_IDLE )  return;
    // Update Phase
    if( MotorUpdatePhase( pMtr ) == 1 ){
        MotorSetup( pMtr );
        MotorOutput( pMtr );
        MotorUpdateCurrentPosition( pMtr );
    }
    // Update PPS Timer
    MotorUpdatePPSTimer( pMtr );
    // Update status for next process
    MotorUpdateNextStatus( pMtr );
}

// function : Update for motor Next status
static void MotorUpdateNextStatus( MOTOR_INFO* const pMtr )
{
    switch( pMtr->status ){
        default:
        case MTS_IDLE:
        case MTS_RUN_ACCEL:
        case MTS_RUN_DECEL:
            break;
        case MTS_RUN_CONST:
            if( pMtr->target_position == pMtr->motor_position ) pMtr->status = MTS_BREAK;
            break;
        case MTS_BREAK:
            if( pMtr->break_timer == 0 ) pMtr->status = MTS_IDLE;
            break;
    }
}

// function : Update for Phase Index
static uint32_t MotorUpdatePhase( MOTOR_INFO* const pMtr )
{
    // Phase Index
    // Check Constant stepping
    if( MotorUpdatePhaseIfConst(pMtr) == 1 ) return 1;
    // Check Breaking timeout
    if( MotorUpdatePhaseIfBreak(pMtr) == 1 ) return 1;

    return 0;
}
static uint32_t MotorUpdatePhaseIfConst( MOTOR_INFO* const pMtr )
{
    if( pMtr->status != MTS_RUN_CONST )  return 0;
    if( pMtr->pps_timer != ((uint32_t)(INTERRUPT_TIMER_INTERVAL / pMtr->pps)) ) return 0;

    // Update
    pMtr->phase_index += pMtr->phase_index_update_num;
    pMtr->phase_index &= MOTOR_PHASE_MASK;
    return 1;
}

static uint32_t MotorUpdatePhaseIfBreak( MOTOR_INFO* const pMtr )
{
    if( pMtr->status != MTS_BREAK )  return 0;
    
    // Check Breaking timeout
    if( pMtr->break_timer > 0 ){
        pMtr->break_timer--;
    }
 
    // break_timer = 0 then output off and change status to IDLE
    // break_timer > 0 then output keep
    if( pMtr->break_timer == 0 ){
        pMtr->phase_pos     = pMtr->phase_index;
        pMtr->phase_index   = MOTOR_OFF_INDEX;
        return 1;
    }
    else{
        return 0;
    }
}

// function : Update for Current Position
static void MotorUpdateCurrentPosition( MOTOR_INFO* const pMtr )
{
    //if( pMtr->status == MTS_BREAK || pMtr->status == MTS_IDLE ) return;
    if( pMtr->phase_mode == MTP_PHASE_HALF ){
        // now HALF-STEP position then return
        if( pMtr->phase_pos%2 ) return;
    }

    // Update
    if( pMtr->direction == MTD_CW ) pMtr->motor_position++;
    else                            pMtr->motor_position--;
}

// function : Update for PPS Timer
static void MotorUpdatePPSTimer( MOTOR_INFO* const pMtr )
{
    if( pMtr->status != MTS_RUN_CONST )  return;

    // Check Phase Update time
    pMtr->pps_timer--;
    if( pMtr->pps_timer == 0 ){
        // Reset pps_timer
        pMtr->pps_timer = (uint32_t)(INTERRUPT_TIMER_INTERVAL / pMtr->pps);
    }
}

// function : Desision Phase Index Update Number
static void MotorDecisionPhaseIndexUpdateNumber( MOTOR_INFO* const pMtr )
{
    // Phase mode
    if( pMtr->phase_mode == MTP_PHASE_FULL )    pMtr->phase_index_update_num = 2;   // FULL-STEP
    else                                        pMtr->phase_index_update_num = 1;   // HALF-STEP
    // Direction
    if( pMtr->direction == MTD_CCW )            pMtr->phase_index_update_num *= -1;
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
        motors[nMotor].pps           = DEFAULT_PPS;
        motors[nMotor].pps_timer     = (uint32_t)(INTERRUPT_TIMER_INTERVAL/DEFAULT_PPS);
        motors[nMotor].phase_mode    = MTP_PHASE_FULL;
        motors[nMotor].phase_index_update_num = 2;
        motors[nMotor].phase_index   = MOTOR_OFF_INDEX;
        motors[nMotor].phase_pos     = 0;
        motors[nMotor].break_timeout = 10,
        motors[nMotor].break_timer = 0,
        motors[nMotor].motor_position   = 0; 
        motors[nMotor].target_position  = 0; 
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
        //motors[nMotor].phase_pos     = 6;
        //motors[nMotor].motor_position= -1; 
        //MotorMove( 0, 1000, 12 );
        //MotorMove( 0, 1000, -12 );
        //MotorMove( 0, 1000, 0 );
        //MotorMove( 0, 500, 4 );
        //MotorMove( 0, 500, -4 );
    }
}

// function : Control for Motor output status
void MotorControl( void )
{
    for(uint16_t nMotor=0; nMotor < MOTOR_MAX; nMotor++ ){
        MotorUpdate( &(motors[nMotor]) );
    }
}

// function : Setup for Moving
void MotorMove( uint16_t nMotor, uint32_t pps, int32_t position )
{
    if( nMotor > (MOTOR_MAX - 1) )          return; 
    if( pps == 0 )                          return; 
    if( pps >  INTERRUPT_TIMER_INTERVAL )   return; 

    // Disable Interrupt

    // PPS Setup
    motors[nMotor].pps          = pps;
    motors[nMotor].pps_timer    = (uint32_t)(INTERRUPT_TIMER_INTERVAL / pps);

    // Direction and target position Setup
    if( position >= motors[nMotor].motor_position ) motors[nMotor].direction     = MTD_CW;
    else                                            motors[nMotor].direction     = MTD_CCW;
    motors[nMotor].target_position = position;

    // Phase Setup
    MotorDecisionPhaseIndexUpdateNumber( &motors[nMotor] );  

    // Start
    motors[nMotor].phase_index  = motors[nMotor].phase_pos;
    motors[nMotor].break_timer  = motors[nMotor].break_timeout;
    if( position == motors[nMotor].motor_position ) motors[nMotor].status = MTS_BREAK;
    else                                            motors[nMotor].status = MTS_RUN_CONST;

    // Enable Interrupt

}