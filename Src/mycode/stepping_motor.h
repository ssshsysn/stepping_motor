
// Phase mode
typedef enum {
    MTP_PHASE_FULL     = 0,  // FULL-STEP Phase mode
    MTP_PHASE_HALF,          // HALF-STEP Phase mode
}PHASE_MODE;

void MotorInitialize( void );
void MotorControl( void );
void MotorMove( uint16_t nMotor, uint32_t pps, int32_t position );
uint32_t MotorIsBusy( uint16_t nMotor );
void MotorResetPosition( uint16_t nMotor );
void MotorSetPhaseMode( uint16_t nMotor, PHASE_MODE phase_mode );