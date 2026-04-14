#include <stdint.h>

/* ================= RCC ================= */
#define RCC_BASE        0x40023800
#define RCC_AHB1ENR     (*(volatile uint32_t*)(RCC_BASE + 0x30))
#define RCC_APB1ENR     (*(volatile uint32_t*)(RCC_BASE + 0x40))

/* ================= GPIO ================= */
#define GPIOA_BASE      0x40020000
#define GPIOC_BASE      0x40020800

#define GPIOA_MODER     (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
#define GPIOA_IDR       (*(volatile uint32_t*)(GPIOA_BASE + 0x10))
#define GPIOA_AFRL      (*(volatile uint32_t*)(GPIOA_BASE + 0x20))

#define GPIOC_MODER     (*(volatile uint32_t*)(GPIOC_BASE + 0x00))
#define GPIOC_ODR       (*(volatile uint32_t*)(GPIOC_BASE + 0x14))

/* ================= USART2 ================= */
#define USART2_BASE     0x40004400
#define USART2_SR       (*(volatile uint32_t*)(USART2_BASE + 0x00))
#define USART2_DR       (*(volatile uint32_t*)(USART2_BASE + 0x04))
#define USART2_BRR      (*(volatile uint32_t*)(USART2_BASE + 0x08))
#define USART2_CR1      (*(volatile uint32_t*)(USART2_BASE + 0x0C))

#define NVIC_ISER1      (*(volatile uint32_t*)(0xE000E104))

/* ================= TIM3 ================= */
/* PA6 = TIM3_CH1, PA7 = TIM3_CH2 */
#define TIM3_BASE       0x40000400
#define TIM3_CR1        (*(volatile uint32_t*)(TIM3_BASE + 0x00))
#define TIM3_EGR        (*(volatile uint32_t*)(TIM3_BASE + 0x14))
#define TIM3_CCMR1      (*(volatile uint32_t*)(TIM3_BASE + 0x18))
#define TIM3_CCER       (*(volatile uint32_t*)(TIM3_BASE + 0x20))
#define TIM3_PSC        (*(volatile uint32_t*)(TIM3_BASE + 0x28))
#define TIM3_ARR        (*(volatile uint32_t*)(TIM3_BASE + 0x2C))
#define TIM3_CCR1       (*(volatile uint32_t*)(TIM3_BASE + 0x34))
#define TIM3_CCR2       (*(volatile uint32_t*)(TIM3_BASE + 0x38))

/* ================= SYSTICK ================= */
#define SYST_CSR        (*(volatile uint32_t*)(0xE000E010))
#define SYST_RVR        (*(volatile uint32_t*)(0xE000E014))
#define SYST_CVR        (*(volatile uint32_t*)(0xE000E018))

/* ================= CONFIG ================= */
#define SENSOR_PIN      0       // PA0
#define LED_PIN         13      // PC13

#define CMD_NONE        0
#define CMD_A           'A'
#define CMD_B           'B'

/* Servo pulse width (1 tick = 1 us because timer runs at 1 MHz) */
#define SERVO_HOME_US   500     // vị trí ban đầu ~ 0 độ
#define SERVO_180_US    2500    // vị trí 180 độ
#define SERVO_HOLD_MS   700     // giữ ở vị trí 180 trước khi quay về
#define LED_PULSE_MS    500

/* ================= GLOBAL ================= */
volatile char pending_action = 0;
volatile uint8_t action_pending = 0;
volatile uint32_t g_ms_ticks = 0;

volatile uint8_t led_active = 0;
volatile uint32_t led_off_time = 0;

/* ================= TIME ================= */
void SysTick_Init(void)
{
    SYST_RVR = 16000 - 1;  // 16MHz -> 1ms
    SYST_CVR = 0;
    SYST_CSR = (1 << 2) | (1 << 1) | (1 << 0);
}

void SysTick_Handler(void)
{
    g_ms_ticks++;
}

uint32_t millis(void)
{
    return g_ms_ticks;
}

void delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((uint32_t)(millis() - start) < ms)
    {
    }
}

/* ================= LED ================= */
void LED_On(void)
{
    GPIOC_ODR &= ~(1U << LED_PIN);   // PC13 active low
}

void LED_Off(void)
{
    GPIOC_ODR |= (1U << LED_PIN);
}

void LED_Pulse(uint32_t ms)
{
    LED_On();
    led_active = 1;
    led_off_time = millis() + ms;
}

void LED_Update(void)
{
    if (led_active)
    {
        if ((int32_t)(millis() - led_off_time) >= 0)
        {
            LED_Off();
            led_active = 0;
        }
    }
}

/* ================= UART ================= */
void UART2_Init(void)
{
    RCC_APB1ENR |= (1U << 17);               // USART2 enable

    USART2_BRR = 0x0683;                     // 9600 @ 16MHz
    USART2_CR1 |= (1U << 5);                 // RXNE interrupt
    USART2_CR1 |= (1U << 2) | (1U << 3);     // RX, TX
    USART2_CR1 |= (1U << 13);                // USART enable

    NVIC_ISER1 |= (1U << (38 - 32));         // USART2 IRQ
}

void USART2_IRQHandler(void)
{
    if (USART2_SR & (1U << 5))
    {
        char data = (char)USART2_DR;

        if (data == CMD_A || data == CMD_B)
        {
            pending_action = data;
            action_pending = 1;
        }
    }
}

/* ================= GPIO ================= */
void GPIO_Init(void)
{
    RCC_AHB1ENR |= (1U << 0) | (1U << 2);    // GPIOA, GPIOC

    /* PA0 input - IR sensor */
    GPIOA_MODER &= ~(3U << (SENSOR_PIN * 2));

    /* PA2, PA3 -> USART2 AF7 */
    GPIOA_MODER &= ~(0xFU << 4);
    GPIOA_MODER |=  (0xAU << 4);

    GPIOA_AFRL  &= ~(0xFFU << 8);
    GPIOA_AFRL  |=  (0x77U << 8);

    /* PA6, PA7 -> TIM3 AF2 */
    GPIOA_MODER &= ~(0xFU << 12);
    GPIOA_MODER |=  (0xAU << 12);

    GPIOA_AFRL  &= ~(0xFFU << 24);
    GPIOA_AFRL  |=  (0x22U << 24);

    /* PC13 output */
    GPIOC_MODER &= ~(3U << (LED_PIN * 2));
    GPIOC_MODER |=  (1U << (LED_PIN * 2));

    LED_Off();
}

/* ================= PWM ================= */
void TIM3_PWM_Init(void)
{
    RCC_APB1ENR |= (1U << 1);                // TIM3 enable

    TIM3_CR1   = 0;
    TIM3_CCMR1 = 0;
    TIM3_CCER  = 0;

    TIM3_PSC = 16 - 1;                       // 1MHz
    TIM3_ARR = 20000 - 1;                    // 20ms -> 50Hz

    /* CH1 PWM mode 1 + preload */
    TIM3_CCMR1 |= (6U << 4) | (1U << 3);

    /* CH2 PWM mode 1 + preload */
    TIM3_CCMR1 |= (6U << 12) | (1U << 11);

    /* Enable CH1 + CH2 */
    TIM3_CCER |= (1U << 0) | (1U << 4);

    /* vị trí ban đầu */
    TIM3_CCR1 = SERVO_HOME_US;
    TIM3_CCR2 = SERVO_HOME_US;

    TIM3_EGR |= 1U;
    TIM3_CR1 |= (1U << 7);
    TIM3_CR1 |= (1U << 0);
}

/* ================= SERVO ================= */
void servo1_go_180_then_home(void)
{
    TIM3_CCR1 = SERVO_180_US;
    delay_ms(SERVO_HOLD_MS);
    TIM3_CCR1 = SERVO_HOME_US;
}

void servo2_go_180_then_home(void)
{
    TIM3_CCR2 = SERVO_180_US;
    delay_ms(SERVO_HOLD_MS);
    TIM3_CCR2 = SERVO_HOME_US;
}

/* ================= APP ================= */
void App_UpdateServo(void)
{
    uint8_t sensor_active = ((GPIOA_IDR & (1U << SENSOR_PIN)) == 0);

    if (sensor_active && action_pending)
    {
        if (pending_action == CMD_A)
        {
        	delay_ms(2000);
            servo1_go_180_then_home();
            LED_Pulse(LED_PULSE_MS);
            action_pending = 0;
        }
        else if (pending_action == CMD_B)
        {
        	delay_ms(3000);
            servo2_go_180_then_home();
            LED_Pulse(LED_PULSE_MS);
            action_pending = 0;
        }
    }
}

/* ================= MAIN ================= */
int main(void)
{
    GPIO_Init();
    UART2_Init();
    TIM3_PWM_Init();
    SysTick_Init();

    while (1)
    {
        App_UpdateServo();
        LED_Update();
    }
}
