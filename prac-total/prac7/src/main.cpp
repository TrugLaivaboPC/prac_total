#define F_CPU 16000000UL 
#include <avr/io.h>

#include <avr/interrupt.h>

#include <util/delay.h>

#include <stdint.h>





#define LCD_PORT PORTD

#define LCD_DDR  DDRD



#define RS PD2

#define EN PD3

#define D4 PD4

#define D5 PD5

#define D6 PD6

#define D7 PD7



volatile uint16_t capture_previous = 0;

volatile uint16_t capture_period = 0;



volatile uint8_t capture_ready = 0;

volatile uint8_t first_capture = 1;





void LCD_PulseEnable(void)

{

    LCD_PORT |= (1 << EN);

    _delay_us(1);



    LCD_PORT &= ~(1 << EN);

    _delay_us(100);

}



void LCD_SendNibble(uint8_t data)

{

    // Clear D4-D7

    LCD_PORT &= ~((1 << D4) |

                  (1 << D5) |

                  (1 << D6) |

                  (1 << D7));



    // Put data on D4-D7

    if (data & 0x01)

        LCD_PORT |= (1 << D4);



    if (data & 0x02)

        LCD_PORT |= (1 << D5);



    if (data & 0x04)

        LCD_PORT |= (1 << D6);



    if (data & 0x08)

        LCD_PORT |= (1 << D7);



    LCD_PulseEnable();

}





void LCD_Command(uint8_t cmd)

{

    LCD_PORT &= ~(1 << RS);



    LCD_SendNibble(cmd >> 4);

    LCD_SendNibble(cmd & 0x0F);



    _delay_ms(2);

}





void LCD_Data(uint8_t data)

{

    LCD_PORT |= (1 << RS);



    LCD_SendNibble(data >> 4);

    LCD_SendNibble(data & 0x0F);



    _delay_us(50);

}





void LCD_Init(void)

{

    LCD_DDR |= (1 << RS) |

               (1 << EN) |

               (1 << D4) |

               (1 << D5) |

               (1 << D6) |

               (1 << D7);



    _delay_ms(20);



    LCD_PORT &= ~(1 << RS);



    // HD44780 initialization

    LCD_SendNibble(0x03);

    _delay_ms(5);



    LCD_SendNibble(0x03);

    _delay_us(150);



    LCD_SendNibble(0x03);

    _delay_us(150);



    // Switch to 4-bit mode

    LCD_SendNibble(0x02);



    // 4-bit, 2 lines, 5x8 font

    LCD_Command(0x28);



    // Display ON, cursor OFF

    LCD_Command(0x0C);



    // Entry mode

    LCD_Command(0x06);



    // Clear display

    LCD_Command(0x01);



    _delay_ms(2);

}





void LCD_Clear(void)

{

    LCD_Command(0x01);

    _delay_ms(2);

}





void LCD_SetCursor(uint8_t row, uint8_t column)

{

    uint8_t address;



    if (row == 0)

        address = 0x00 + column;

    else

        address = 0x40 + column;



    LCD_Command(0x80 | address);

}





void LCD_Print(const char *str)

{

    while (*str)

    {

        LCD_Data(*str);

        str++;

    }

}





void LCD_PrintNumber(uint16_t number)

{

    char buffer[6];

    uint8_t i = 0;



    if (number == 0)

    {

        LCD_Data('0');

        return;

    }



    while (number > 0)

    {

        buffer[i++] = '0' + (number % 10);

        number /= 10;

    }



    while (i > 0)

    {

        LCD_Data(buffer[--i]);

    }

}



void InputCapture_Init(void)

{

    /*

        ICP1 = PB0



        PB0 is an INPUT.

    */



    DDRB &= ~(1 << PB0);



    /*

        Timer1 Normal Mode



        WGM13:0 = 0000

    */
TCCR1A = 0;



    /*

        ICES1 = 1

        Capture on rising edge



        CS11 = 1

        Timer1 prescaler = 8

    */



    TCCR1B =

        (1 << ICES1) |

        (1 << CS11);



    /*

        Enable Timer1 Input Capture interrupt

    */



    TIMSK1 |= (1 << ICIE1);



    // Start timer from zero

    TCNT1 = 0;

}





// ======================================================

// INPUT CAPTURE INTERRUPT

// ======================================================



ISR(TIMER1_CAPT_vect)

{

    uint16_t current_capture;



    // Read captured Timer1 value

    current_capture = ICR1;



    if (first_capture)

    {

        /*

            First edge:

            only store its timestamp.

        */



        capture_previous = current_capture;



        first_capture = 0;

    }

    else

    {

        /*

            Second edge:

            calculate time between edges.

        */



        capture_period =

            current_capture - capture_previous;



        capture_previous = current_capture;



        /*

            Tell main() that a new

            measurement is available.

        */



        capture_ready = 1;

    }

}



int main(void)

{

    uint16_t period;

    uint32_t frequency;



    LCD_Init();



    InputCapture_Init();



    // Enable global interrupts

    sei();



    LCD_Clear();



    LCD_SetCursor(0, 0);

    LCD_Print("Waiting...");



    while (1)

    {

        if (capture_ready)

        {

            /*

                Copy the captured period.

            */



            capture_ready = 0;

            period = capture_period;

            if (period != 0)

            {

                /*

                    Frequency:



                    f = Timer frequency / period



                    f = 2,000,000 / period

                */



                frequency =

                    2000000UL / period;

                 

                LCD_SetCursor(0, 0);



                LCD_Print("CAP: ");

                LCD_PrintNumber(period);



                LCD_Print("      ");



                LCD_SetCursor(1, 0);



                LCD_Print("F: ");

                LCD_PrintNumber((uint16_t)frequency);

                LCD_Print(" Hz   ");

            }

        }

    }

}