

// main.c

#include <rtthread.h>

#include "function1_display_thread.h"
#include "event.h"
#include "function1_ctrl.h"
#include "function2_display_thread.h"
#include "display_function_thread.h"
#include "uart6_rx.h"
#include <packages/ssd1306-latest/include/ssd1306.h>

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

void begin_surface(void)
{
    char *begin_string = "function1";

    /* 初始化 OLED */
    ssd1306_Init();
    ssd1306_Fill(Black);

    // 显示文字
    ssd1306_SetCursor(2, 20 + 10);
    ssd1306_WriteString(begin_string, Font_7x10, White);
//
//    // 向后箭头
//    ssd1306_Line(14, 31, 18, 37, White);
//    ssd1306_Line(18, 24, 18, 36, White);
//    ssd1306_Line(22, 31, 18, 37, White);


    /* 刷新屏幕 */
    ssd1306_UpdateScreen();
}

int main(void)
{
    // 初始化
    event_init();
    uart6_rx_init();
    function1_ctrl_init();
    function2_display_thread_init();
    display_function_thread_init();

    // 初始显示
    begin_surface();

    return RT_EOK;
}
