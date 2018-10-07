#define PHASE_A1    (0)
#define PHASE_B1    (1)
#define PHASE_A2    (2)
#define PHASE_B2    (3)
#define PHASE_MAX   (4)

// Motor pin information
typedef struct {
    GPIO_TypeDef*       port;           // GPIO PORT NUMBER
    uint16_t            pin;            // GPIO PIN NUMBER
    GPIO_PinState       output;         // PIN STATE(GPIO_PIN_SET,GPIO_PIN_RESET)         
}MOTOR_PIN_INFO;
// Motor information
typedef struct {
    MOTOR_PIN_INFO      phase[PHASE_MAX];       // phase information
    uint32_t            phase_index;            // phase current index
    int32_t             pos;                    // motor position                    
}MOTOR_INFO;

void MotorInitialize( void );
void MotorControl( void );
void ToggleLed( void );