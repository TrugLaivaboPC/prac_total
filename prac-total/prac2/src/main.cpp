#define F_CPU 16000000UL // Tần số thạch anh 16MHz
#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

// Định nghĩa chân LCD
#define LCD_CTRL_PORT PORTB
#define LCD_CTRL_DDR  DDRB
#define LCD_DATA_PORT PORTD
#define LCD_DATA_DDR  DDRD

#define RS_PIN PB0
#define EN_PIN PB1
#define RW_PIN PB2

/* ================= KHỐI ĐIỀU KHIỂN LCD (4-BIT) ================= */

void LCD_Pulse_Enable(void) {
    LCD_CTRL_PORT |= (1 << EN_PIN);
    _delay_us(1);
    LCD_CTRL_PORT &= ~(1 << EN_PIN);
    _delay_us(100);
}

void LCD_Send4Bits(uint8_t data) {
    // Xóa 4 bit cao của PORTD (PD4-PD7) và ghi 4 bit data vào
    LCD_DATA_PORT = (LCD_DATA_PORT & 0x0F) | (data & 0xF0);
    LCD_Pulse_Enable();
}

void LCD_Command(uint8_t cmd) {
    LCD_CTRL_PORT &= ~(1 << RS_PIN); // RS = 0 (Lệnh)
    LCD_Send4Bits(cmd);              // Gửi 4 bit cao
    LCD_Send4Bits(cmd << 4);         // Gửi 4 bit thấp
    _delay_ms(2);
}

void LCD_Char(char data) {
    LCD_CTRL_PORT |= (1 << RS_PIN);  // RS = 1 (Dữ liệu)
    LCD_Send4Bits(data);             // Gửi 4 bit cao
    LCD_Send4Bits(data << 4);        // Gửi 4 bit thấp
    _delay_us(100);
}

void LCD_String(const char *str) {
    while (*str) {
        LCD_Char(*str++);
    }
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t address = (row == 0) ? (0x80 + col) : (0xC0 + col);
    LCD_Command(address);
}

void LCD_Init(void) {
    // Cấu hình chân Output
    LCD_CTRL_DDR |= (1 << RS_PIN) | (1 << EN_PIN) | (1 << RW_PIN);
    LCD_DATA_DDR |= 0xF0; // PD4, PD5, PD6, PD7 làm Output

    LCD_CTRL_PORT &= ~(1 << RW_PIN); // RW = 0 (Chế độ ghi)
    _delay_ms(20);

    // Khởi tạo chế độ 4-bit theo chuẩn HD44780
    LCD_Send4Bits(0x30);
    _delay_ms(5);
    LCD_Send4Bits(0x30);
    _delay_us(200);
    LCD_Send4Bits(0x30);
    _delay_us(200);
    LCD_Send4Bits(0x20); // Chuyển sang 4-bit mode
    _delay_ms(2);

    LCD_Command(0x28); // 2 dòng, font 5x8, giao tiếp 4-bit
    LCD_Command(0x0C); // Bật hiển thị, tắt con trỏ
    LCD_Command(0x06); // Tự động tăng con trỏ
    LCD_Command(0x01); // Xóa màn hình
    _delay_ms(2);
}

/* ================= KHỐI ĐIỀU KHIỂN ADC ================= */

void ADC_Init(void) {
    // Chọn Vref = AVcc (tụ lọc ở AREF), kênh đọc ADC0 (PC0)
    ADMUX = (1 << REFS0);

    // Bật ADC (ADEN), chọn Prescaler = 128 (16MHz / 128 = 125kHz, tối ưu trong dải 50-200kHz)
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t ADC_ReadChannel(uint8_t channel) {
    // Giữ REFS0 và chọn kênh 0-7
    ADMUX = (ADMUX & 0xF8) | (channel & 0x07);

    // Bắt đầu chuyển đổi (Start Conversion)
    ADCSRA |= (1 << ADSC);

    // Chờ cờ ADSC về 0 (Polling)
    while (ADCSRA & (1 << ADSC));

    // Trả về kết quả 10-bit (ADC = ADCL + (ADCH << 8))
    return ADC;
}

uint16_t ADC_Read_Average16(uint8_t channel) {
    uint32_t sum = 0;
for (uint8_t i = 0; i < 16; i++) {
        sum += ADC_ReadChannel(channel);
        _delay_us(500); // Đợi ổn định giữa các lần lấy mẫu
    }
    return (uint16_t)(sum >> 4); // Chia 16
}

/* ================= CHƯƠNG TRÌNH CHÍNH ================= */

int main(void) {
    LCD_Init();
    ADC_Init();

    char line1[16];
    char line2[16];

    while (1) {
        // 1. Đọc ADC kênh 0 (PC0), lấy trung bình 16 mẫu
        uint16_t raw_avg = ADC_Read_Average16(0);

        // 2. Chuyển đổi sang điện áp (Vref = 5.0V, 10-bit: 1023)
        // Tính bằng số nguyên mV để tránh phụ thuộc thư viện float cồng kềnh
        uint32_t volt_mv = ((uint32_t)raw_avg * 5000) / 1023;
        uint16_t volt_int = volt_mv / 1000;      // Phần nguyên (Volt)
        uint16_t volt_dec = (volt_mv % 1000) / 10;
        uint16_t light_percent = ((uint32_t)raw_avg*100)/1023;

        sprintf(line1, "light:%4u %u.%0.2uV",raw_avg,volt_int,volt_dec);

        if(light_percent >50){
          sprintf(line2, "b:%3u%% [DAY]", light_percent);

        }
        else if (light_percent > 20){
          sprintf(line2, "b: %3u%%  [DIM]",light_percent);
        }
        else{
          sprintf(line2, "b: %3u%% [DARK]",light_percent);

        }


        // 4. Cập nhật lên LCD
        LCD_SetCursor(0, 0);
LCD_String("                ");
LCD_SetCursor(0, 0);
LCD_String(line1);

LCD_SetCursor(1, 0);
LCD_String("                ");
LCD_SetCursor(1, 0);
LCD_String(line2);
 // Tốc độ quét cập nhật màn hình (~4Hz)
    }

    return 0;
}




