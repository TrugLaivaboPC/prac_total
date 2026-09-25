#include <avr/io.h>

#include <util/delay.h>

#include <stdint.h>



#define PUL PB0

#define DIR PB2

#define EN  PB3



void step_motor(void)

{

    PORTB |= (1 << PUL);

    _delay_us(10);



    PORTB &= ~(1 << PUL);

    _delay_ms(5);

}



int main(void)

{

    DDRB |= (1 << PUL) |

            (1 << DIR) |

            (1 << EN);



    // Enable

    PORTB &= ~(1 << EN);



    // Direction

    PORTB |= (1 << DIR);



    while (1)

    {

        // 400 pulses = 90° at 1/8 microstep

        for (uint16_t i = 0; i < 400; i++)

        {

            step_motor();

        }



        _delay_ms(1000);



        // Reverse

        PORTB ^= (1 << DIR);

    }

}