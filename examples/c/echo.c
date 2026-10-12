#include "../../sdk/esp32.h"

// Echoes each line typed in the web console, unchanged.
ESP_ENTRY void run(void)
{
    char line[128];
    int n = 0;

    print_str("type something and press Enter\n");
    for (;;)
    {
        int c = read_char();
        if (c < 0)
        {
            delay_ms(10);
            continue;
        }
        if (c == '\n')
        {
            line[n++] = '\n';
            print_str("you said: ");
            print(line, n);
            n = 0;
        }
        else if (n < (int)sizeof(line) - 1)
        {
            line[n++] = c;
        }
    }
}
