/*
 * Practice 11 - Do cuong do anh sang bang LDR - board TDC-3628 (ATmega328P)
 * Phien ban cho LED 7 doan co 8 duong doan a..h  (h = dau cham dp)
 *
 * Bang dau day:
 *   LDR  (khoi "CB anh sang", J40)   -> PC0 / A0
 *   Doan a b c d e f                 -> PD2 PD3 PD4 PD5 PD6 PD7
 *   Doan g                           -> PB0
 *   Doan h (dp)                      -> PB1
 *   Chon digit D1 D2 D3 D4           -> PB2 PB3 PB4 PB5
 *   LED bao toi (khoi "Led don" L1)  -> PC1
 *   UART -> cong USB                 -> PD0 (RX), PD1 (TX)  [khong dau day]
 *
 * Bien dich:
 *   avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL -Os -o ldr.elf ldr_tdc3628_8seg.c
 *   avr-objcopy -O ihex -R .eeprom ldr.elf ldr.hex
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

/* ================== CAU HINH ================== */

/* 1 = anode chung / lai qua PNP (doan sang khi chan muc THAP)
   0 = cathode chung (doan sang khi chan muc CAO)               */
#define SEG_ACTIVE_LOW    1

/* 1 = digit duoc chon khi chan muc THAP, 0 = muc CAO           */
#define DIGIT_ACTIVE_LOW  1

/* 0 = hien thi gia tri ADC tho (0..1023)
   1 = hien thi dien ap dang d.ddd V, dung dau cham tren digit 1 */
#define DISPLAY_MODE      0

#define BAUD              9600UL
#define UBRR_VALUE        ((F_CPU / (16UL * BAUD)) - 1UL)   /* = 103 */

#define LDR_CHANNEL       0        /* ADC0 = PC0 */
#define VREF_MV           5000UL

/* Nguong co tre (hysteresis) - PHAI hieu chinh theo LDR thuc te */
#define TH_DARK           400      /* ADC < 400 -> TOI  */
#define TH_BRIGHT         480      /* ADC > 480 -> SANG */

#define LED_PIN           PC1      /* LED bao toi */

/* Chan doan phu: g tren PB0, h (dp) tren PB1 */
#define SEG_G_PIN         PB0
#define SEG_H_PIN         PB1
#define DIGIT_FIRST_PIN   PB2      /* D1=PB2, D2=PB3, D3=PB4, D4=PB5 */
#define DIGIT_MASK        ((1 << PB2) | (1 << PB3) | (1 << PB4) | (1 << PB5))

/* ================== BANG MA 7 DOAN ================== */
/* bit0=a, bit1=b, ... bit6=g, bit7=h(dp) - dang cathode chung goc */
static const uint8_t seg_code[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

#define BLANK 0xFF   /* ma dac biet: tat digit */

static volatile uint8_t disp_buf[4] = {BLANK, BLANK, BLANK, 0};
static volatile uint8_t dp_mask     = 0;   /* bit i = 1 -> bat dau cham o digit i */

/* ================== UART ================== */
static void uart_init(void)
{
    UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)(UBRR_VALUE);
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);   /* 8N1 */
}

static void uart_putc(char c)
{
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    UDR0 = c;
}

static void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

static void uart_put_u16(uint16_t value, uint8_t width)
{
    char buf[6];
    int8_t i = 5;

    buf[5] = '\0';
    do {
        buf[--i] = (char)('0' + (value % 10));
        value /= 10;
    } while (value && i > 0);

    while ((5 - i) < width && i > 0)
        buf[--i] = '0';

    uart_puts(&buf[i]);
}

/* ================== ADC ================== */
static void adc_init(void)
{
    ADMUX  = (1 << REFS0);                             /* tham chieu AVcc */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    DIDR0 |= (1 << LDR_CHANNEL);                       /* tat bo dem so tren PC0 */

    ADCSRA |= (1 << ADSC);                             /* bo lan chuyen doi dau */
    while (ADCSRA & (1 << ADSC))
        ;
}

static uint16_t adc_read(uint8_t channel)
{
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;
    return ADC;
}

static uint16_t adc_read_avg(uint8_t channel)
{
    uint32_t sum = 0;
    uint8_t i;

    for (i = 0; i < 8; i++) {
        sum += adc_read(channel);
        _delay_us(200);
    }
    return (uint16_t)(sum >> 3);
}

/* ================== 7 DOAN ================== */
static void seg_all_digits_off(void)
{
#if DIGIT_ACTIVE_LOW
    PORTB |= DIGIT_MASK;
#else
    PORTB &= (uint8_t)~DIGIT_MASK;
#endif
}

static void seg_digit_on(uint8_t d)          /* d = 0..3 */
{
#if DIGIT_ACTIVE_LOW
    PORTB &= (uint8_t)~(1 << (DIGIT_FIRST_PIN + d));
#else
    PORTB |= (uint8_t)(1 << (DIGIT_FIRST_PIN + d));
#endif
}

/* Xuat ma 8 doan: a..f -> PD2..PD7, g -> PB0, h -> PB1 */
static void seg_write(uint8_t pattern)
{
#if SEG_ACTIVE_LOW
    pattern = (uint8_t)(~pattern);
#endif
    PORTD = (uint8_t)((PORTD & 0x03) | ((pattern & 0x3F) << 2));

    if (pattern & 0x40)
        PORTB |= (1 << SEG_G_PIN);
    else
        PORTB &= (uint8_t)~(1 << SEG_G_PIN);

    if (pattern & 0x80)
        PORTB |= (1 << SEG_H_PIN);
    else
        PORTB &= (uint8_t)~(1 << SEG_H_PIN);
}

static void seg_init(void)
{
    DDRD |= 0xFC;                    /* PD2..PD7 xuat, giu PD0/PD1 cho UART */
    DDRB |= (1 << SEG_G_PIN) | (1 << SEG_H_PIN) | DIGIT_MASK;

    DDRC  |= (1 << LED_PIN);         /* PC0 van la ngo vao ADC */
    PORTC &= (uint8_t)~(1 << LED_PIN);

    seg_all_digits_off();
    seg_write(0x00);                 /* tat het doan */
}

/* Timer0 CTC: 16 MHz / 64 / 250 = 1 kHz -> 1 ms/digit -> 250 Hz toan man hinh */
static void timer0_init(void)
{
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS01) | (1 << CS00);
    OCR0A  = 249;
    TIMSK0 = (1 << OCIE0A);
}

ISR(TIMER0_COMPA_vect)
{
    static uint8_t digit = 0;
    uint8_t value, pattern;

    seg_all_digits_off();            /* tat truoc khi doi ma -> chong bong ma */

    digit = (uint8_t)((digit + 1) & 0x03);
    value = disp_buf[digit];
    pattern = (value > 9) ? 0x00 : seg_code[value];

    if (dp_mask & (1 << digit))
        pattern |= 0x80;             /* bat doan h */

    seg_write(pattern);

    if (pattern)                     /* digit trong thi khoi bat */
        seg_digit_on(digit);
}

/*
 * n         : so can hien thi (0..9999)
 * dots      : mat na dau cham, bit0 = digit trai nhat
 * blank_lead: 1 = xoa so 0 vo nghia o dau, 0 = hien du 4 chu so
 */
static void display_number(uint16_t n, uint8_t dots, uint8_t blank_lead)
{
    uint8_t d[4];

    if (n > 9999)
        n = 9999;

    d[3] = (uint8_t)(n % 10);
    d[2] = (uint8_t)((n / 10) % 10);
    d[1] = (uint8_t)((n / 100) % 10);
    d[0] = (uint8_t)((n / 1000) % 10);

    if (blank_lead) {
        if (d[0] == 0) {
            d[0] = BLANK;
            if (d[1] == 0) {
                d[1] = BLANK;
                if (d[2] == 0)
                    d[2] = BLANK;
            }
        }
    }

    cli();
    disp_buf[0] = d[0];
    disp_buf[1] = d[1];
    disp_buf[2] = d[2];
    disp_buf[3] = d[3];
    dp_mask     = dots;
    sei();
}

/* ================== MAIN ================== */
int main(void)
{
    uint16_t adc_value, mv;
    uint8_t  is_dark = 0;

    seg_init();
    adc_init();
    uart_init();
    timer0_init();
    sei();

    uart_puts("\r\n=== LDR Light Meter - TDC-3628 ===\r\n");

    while (1) {
        adc_value = adc_read_avg(LDR_CHANNEL);
        mv = (uint16_t)(((uint32_t)adc_value * VREF_MV) / 1023UL);

        if (adc_value < TH_DARK)
            is_dark = 1;
        else if (adc_value > TH_BRIGHT)
            is_dark = 0;
        /* nam giua hai nguong: giu nguyen trang thai cu */

        if (is_dark)
            PORTC |= (1 << LED_PIN);
        else
            PORTC &= (uint8_t)~(1 << LED_PIN);

#if DISPLAY_MODE == 0
        display_number(adc_value, 0x00, 1);      /* so ADC tho, xoa so 0 dau */
#else
        display_number(mv, 0x01, 0);             /* d.ddd V, cham sau digit 1 */
#endif

        uart_puts("ADC=");
        uart_put_u16(adc_value, 4);
        uart_puts("  V=");
        uart_put_u16(mv / 1000, 1);
        uart_putc('.');
        uart_put_u16(mv % 1000, 3);
        uart_puts("V  STATE=");
        uart_puts(is_dark ? "DARK   (LED ON)\r\n" : "BRIGHT (LED OFF)\r\n");

        _delay_ms(500);
    }
}