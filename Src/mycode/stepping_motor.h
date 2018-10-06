// Motor pin information
typedef struct {
    GPIO_TypeDef*       port;           // GPIO PORT NUMBER
    uint16_t            pin;            // GPIO PIN NUMBER
    GPIO_PinState       output;         // PIN STATE(GPIO_PIN_SET,GPIO_PIN_RESET)         
}MOTOR_PIN_INFO;

typedef struct {
    MOTOR_PIN_INFO      phase[4];       // phase information
}MOTOR_INFO;

void ToggleLed( void );