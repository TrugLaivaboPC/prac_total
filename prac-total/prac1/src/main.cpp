#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>


#define BTN_PIN           PD2
#define BTN_PORT_IN       PIND
#define BTN_PORT_DDR      DDRD
#define BTN_PORT_PULLUP   PORTD

#define LED1_PIN          PB0
#define LED2_PIN          PB1
#define BUZZER_PIN        PB4
#define OUT_PORT_DDR      DDRB
#define OUT_PORT          PORTB

#define BUTTON_ACTIVE_LOW 1     /* 1 = nhan xuong GND ; 0 = nhan len VCC */
#define LED_ACTIVE_LOW    0     /* 0 = LED sang khi chan muc cao         */

#define BLINK_PERIOD_MS   500u  /* LED1: 500 ms sang, 500 ms tat         */
#define SAMPLE_PERIOD_MS  5u    /* chu ky lay mau chong doi phim         */


static volatile uint32_t ms_ticks = 0;

/* Timer0 CTC: 16 MHz / 64 / 250 = 1 kHz -> ngat moi 1 ms */
static void timer0_init(void)
{
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS01) | (1 << CS00);
    OCR0A  = 249;
    TIMSK0 = (1 << OCIE0A);
}

ISR(TIMER0_COMPA_vect)
{
    ms_ticks++;
}

static uint32_t millis(void)
{
    uint32_t t;
    cli();
    t = ms_ticks;
    sei();
    return t;
}


static uint8_t  btn_history       = BUTTON_ACTIVE_LOW ? 0xFF : 0x00;
static uint8_t  btn_stable        = 0;   /* trang thai da loc rung */
static uint8_t  btn_prev_stable   = 0;   /* de phat hien canh nhan */

static void button_init(void)
{
    BTN_PORT_DDR &= (uint8_t)~(1 << BTN_PIN);      /* input */
#if BUTTON_ACTIVE_LOW
    BTN_PORT_PULLUP |= (1 << BTN_PIN);              /* bat pull-up noi bo */
#endif
}

/* Goi dinh ky moi SAMPLE_PERIOD_MS. Tra ve 1 neu vua co canh NHAN. */
static uint8_t button_sample(void)
{
    uint8_t raw = (BTN_PORT_IN & (1 << BTN_PIN)) ? 1 : 0;
#if BUTTON_ACTIVE_LOW
    uint8_t pressed_now = raw ? 0 : 1;   /* muc thap = dang nhan */
#else
    uint8_t pressed_now = raw ? 1 : 0;
#endif

    btn_history = (uint8_t)((btn_history << 1) | pressed_now);

    if (btn_history == 0xFF)
        btn_stable = 1;
    else if (btn_history == 0x00)
        btn_stable = 0;
    /* gia tri khac 0x00/0xFF: dang chuyen tiep, giu nguyen trang thai cu */

    uint8_t edge = (uint8_t)(btn_stable && !btn_prev_stable);
    btn_prev_stable = btn_stable;
    return edge;
}


static void output_init(void)
{
    OUT_PORT_DDR |= (1 << LED1_PIN) | (1 << LED2_PIN) | (1 << BUZZER_PIN);

#if LED_ACTIVE_LOW
    OUT_PORT |= (1 << LED1_PIN) | (1 << LED2_PIN);      /* bat dau: tat */
#else
    OUT_PORT &= (uint8_t)~((1 << LED1_PIN) | (1 << LED2_PIN));
#endif
    OUT_PORT &= (uint8_t)~(1 << BUZZER_PIN);
}

static void led_set(uint8_t pin, uint8_t on)
{
#if LED_ACTIVE_LOW
    if (on) OUT_PORT &= (uint8_t)~(1 << pin); else OUT_PORT |= (1 << pin);
#else
    if (on) OUT_PORT |= (1 << pin); else OUT_PORT &= (uint8_t)~(1 << pin);
#endif
}

static void buzzer_set(uint8_t on)
{
    if (on) OUT_PORT |= (1 << BUZZER_PIN);
    else    OUT_PORT &= (uint8_t)~(1 << BUZZER_PIN);
}

/* ================== MAIN ================== */
int main(void)
{
    uint32_t last_sample_time = 0;
    uint32_t last_blink_time  = 0;
    uint8_t  led1_state       = 0;
    uint8_t  led2_state       = 0;

    output_init();
    button_init();
    timer0_init();
    sei();

    while (1) {
        uint32_t now = millis();

        /* ---- LED1: nhay lien tuc, khong dung delay chan ---- */
        if ((uint32_t)(now - last_blink_time) >= BLINK_PERIOD_MS) {
            last_blink_time = now;
            led1_state = (uint8_t)!led1_state;
            led_set(LED1_PIN, led1_state);
        }

        /* ---- Nut bam: lay mau moi SAMPLE_PERIOD_MS, chong doi phim ---- */
        if ((uint32_t)(now - last_sample_time) >= SAMPLE_PERIOD_MS) {
            last_sample_time = now;

            if (button_sample()) {           /* vua co canh nhan */
                led2_state = (uint8_t)!led2_state;
                led_set(LED2_PIN, led2_state);
                buzzer_set(led2_state);       /* buzzer dong bo voi LED2 */
            }
        }
    }
}

