#include "receive.h"
#include "send.h"
#include "wifi.h"

void app_main()
{
    init_uart();
    receive_init();  
    wifi_init();
}