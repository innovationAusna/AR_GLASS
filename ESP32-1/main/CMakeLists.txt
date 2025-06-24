idf_component_register(SRCS "./receive/receive.c" "./http/http.c" "main.c" "./send/send.c" "./wifi/wifi.c" "./cJSON/cJSON.c"
                    INCLUDE_DIRS "." "./receive" "./http" "./send" "./wifi" "./receive" "./cJSON"
                    REQUIRES ${requires}
                    EMBED_TXTFILES 
                    howsmyssl_com_root_cert.pem
                    postman_root_cert.pem
                    "./wifi/wifi_configuration.html")
