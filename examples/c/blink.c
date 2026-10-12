#include "../../sdk/esp32.h"

ESP_ENTRY void run(void)
{

    for (int i = 0;; i++)
    {
        print_str("loop ");
        print_int(i);
        print_str(":\n");

        for (int j = 0; j < 3; j++)
        {
            if (j == 0)
            {
                led_rgb(32, 0, 0);
                print_str("LED red   on -> ");
            }
            else if (j == 1)
            {
                led_rgb(0, 32, 0);
                print_str("LED green on -> ");
            }
            else 
            {
                print_str("LED blue  on -> ");
                led_rgb(0, 0, 32);
            }
            delay_ms(500);

            led_rgb(0, 0, 0);
            print_str("off\n");
            delay_ms(500);
        }
        print_str("\n");
    }
}
