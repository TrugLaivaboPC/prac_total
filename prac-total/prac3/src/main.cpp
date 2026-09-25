#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

/* --- CẤU HÌNH TIMER1 CHO SERVO (CHÂN PB1 / OC1A) --- */
void Servo_PWM_Init(void) {
    // Đặt chân PB1 (OC1A) làm Output
    DDRB |= (1 << PB1);

    // Chế độ Fast PWM Mode 14 (TOP = ICR1)
    // Clear OC1A on Compare Match, set OC1A at BOTTOM (non-inverting)
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11); // Prescaler = 8

    // Đặt chu kỳ 20ms (50Hz)
    ICR1 = 39999; 
}

void Set_Servo_Angle(uint8_t angle) {
    // Chuyển góc (0 - 180) sang giá trị OCR1A (1000us - 2000us -> 2000 - 4000 tick)
    if (angle > 180) angle = 180;
    OCR1A = 2000 + ((uint32_t)angle * 2000) / 180;
}

/* --- CẤU HÌNH TIMER0 CHO ĐỘ SÁNG LED (CHÂN PD6 / OC0A) --- */
void LED_PWM_Init(void) {
    // Đặt chân PD6 (OC0A) làm Output
    DDRD |= (1 << PD6);

    // Fast PWM Mode, Clear on Compare Match (Non-inverting), Prescaler = 64 (~976Hz)
    TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS01) | (1 << CS00);
}

void Set_LED_Brightness(uint8_t brightness) {
    OCR0A = brightness; // 0 (Tắt) -> 255 (Sáng tối đa)
}

/* --- CHƯƠNG TRÌNH CHÍNH --- */
int main(void) {
    Servo_PWM_Init();
    LED_PWM_Init();

    while (1) {
        // Quét góc quay Servo từ 0 đến 180 độ kết hợp tăng độ sáng LED
        for (uint8_t angle = 0; angle <= 180; angle += 10) {
            Set_Servo_Angle(angle);
            Set_LED_Brightness((uint16_t)angle * 255 / 180);
            _delay_ms(100);
        }

        _delay_ms(500);

        // Quét góc quay Servo ngược lại từ 180 về 0 độ
        for (int16_t angle = 180; angle >= 0; angle -= 10) {
            Set_Servo_Angle((uint8_t)angle);
            Set_LED_Brightness((uint16_t)angle * 255 / 180);
            _delay_ms(100);
        }

        _delay_ms(500);
    }

    return 0;
}
