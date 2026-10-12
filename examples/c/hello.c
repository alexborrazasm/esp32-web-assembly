#include "../../sdk/esp32.h"

ESP_ENTRY void run(void)
{
    print_str("Hello from C! uptime ms: ");
    print_int(millis());
    print_str("\n");
}
