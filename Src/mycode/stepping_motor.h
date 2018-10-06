#define PHAZE_A1    (0)
#define PHAZE_B1    (1)
#define PHAZE_A2    (2)
#define PHAZE_B2    (3)
#define PHAZE_MAX   (4)

// Motor pin information
typedef struct {
    GPIO_TypeDef*       port;           // GPIO PORT NUMBER
    uint16_t            pin;            // GPIO PIN NUMBER
    GPIO_PinState       output;         // PIN STATE(GPIO_PIN_SET,GPIO_PIN_RESET)         
}MOTOR_PIN_INFO;
// Motor information
typedef struct {
    MOTOR_PIN_INFO      phase[PHAZE_MAX];       // phase information
}MOTOR_INFO;

void MotorInitialize( void );
void ToggleLed( void );