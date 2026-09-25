#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdio.h>

#define LIGHT_ADC 0
#define GAS_ADC   1

volatile uint8_t f10 = 0;
volatile uint8_t f20 = 0;
volatile uint8_t f100 = 0;
volatile uint8_t f1000 = 0;

volatile uint16_t light_adc = 0;
volatile uint16_t gas_adc = 0;

uint16_t light = 0;
uint16_t gas = 0;
uint8_t light_pct = 0;
uint8_t gas_pct = 0;
uint8_t pwm = 0;

void UART_Init(void)
{
    UBRR0H = 0;
    UBRR0L = 103;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void UART_SendChar(char c)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

void UART_SendString(const char *s)
{
    while (*s)
        UART_SendChar(*s++);
}

void ADC_Init(void)
{
    ADMUX = (1 << REFS0);

    ADCSRA = (1 << ADEN) |
             (1 << ADPS2) |
             (1 << ADPS1) |
             (1 << ADPS0);
}

uint16_t ADC_Read(uint8_t ch)
{
    ADMUX = (1 << REFS0) | (ch & 0x0F);

    ADCSRA |= (1 << ADSC);

    while (ADCSRA & (1 << ADSC));

    return ADC;
}

void Timer0_Init(void)
{
    TCCR0A = (1 << WGM01);

    TCCR0B = (1 << CS01) |
             (1 << CS00);

    OCR0A = 249;

    TIMSK0 = (1 << OCIE0A);
}

ISR(TIMER0_COMPA_vect)
{
    static uint8_t c10 = 0;
    static uint8_t c20 = 0;
    static uint8_t c100 = 0;
    static uint16_t c1000 = 0;

    if (++c10 >= 10)
    {
        c10 = 0;
        f10 = 1;
    }

    if (++c20 >= 20)
    {
        c20 = 0;
        f20 = 1;
    }

    if (++c100 >= 100)
    {
        c100 = 0;
        f100 = 1;
    }

    if (++c1000 >= 1000)
    {
        c1000 = 0;
        f1000 = 1;
    }
}

void PWM_Init(void)
{
    DDRB |= (1 << PB1);

    TCCR1A = (1 << COM1A1) |
             (1 << WGM10);

    TCCR1B = (1 << WGM12) |
             (1 << CS11) |
             (1 << CS10);

    OCR1A = 0;
}

void Task_ADC(void)
{
    light_adc = ADC_Read(LIGHT_ADC);
    gas_adc = ADC_Read(GAS_ADC);
}

void Task_PWM(void)
{
    pwm = gas_adc / 4;
    OCR1A = pwm;
}

void Task_Sensor(void)
{
    light = light_adc;
    gas = gas_adc;

    light_pct = (uint8_t)((uint32_t)light * 100 / 1023);
    gas_pct = (uint8_t)((uint32_t)gas * 100 / 1023);
}

void Task_UART(void)
{
    char buf[120];

    sprintf(buf,
            "Light ADC = %u | Light = %u%% | Gas ADC = %u | Gas = %u%% | PWM = %u\r\n",
            light,
            light_pct,
            gas,
            gas_pct,
            pwm);

    UART_SendString(buf);
}

int main(void)
{
    ADC_Init();
    UART_Init();
    PWM_Init();
    Timer0_Init();

    sei();

    UART_SendString("Practice 5 - Timer and Scheduler\r\n");
    UART_SendString("System started...\r\n\r\n");

    while (1)
    {
        if (f10)
        {
            f10 = 0;
            Task_ADC();
        }

        if (f20)
        {
            f20 = 0;
            Task_PWM();
        }

        if (f100)
        {
            f100 = 0;
            Task_Sensor();
        }

        if (f1000)
        {
            f1000 = 0;
            Task_UART();
        }
    }

    return 0;
}