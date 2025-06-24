// http.h

#ifndef HTTP_H
#define HTTP_H

#include "esp_err.h"
esp_err_t http_chat_with_ai(const char *content, char *out_buf, size_t out_buf_len);

#endif