idf_component_register(SRCS 

"blink_example_main.c" 
"./wifito/wifito.c" 
"./I2S/I2Sto.c" 
"./websocket/webto.c"
"./get_text/text_storage.c"
"./send_to_stm/uart_send_to_stm32.c"
"./http/http.c"

                       INCLUDE_DIRS "." "./wifito" "./I2S" "./websocket" "./get_text" "./send_to_stm" "./http")
