#pragma once
#include <esp_http_server.h>

namespace HttpBasicAuth {
void begin();
bool configured();
bool setPassword(const char *password, size_t length);
bool websocketAuthorized(httpd_handle_t server, int fd);
// No request bodies are read and no credential/header values are logged.
bool authorize(httpd_req_t *request);
esp_err_t dispatch(httpd_req_t *request, esp_err_t (*handler)(httpd_req_t *));
}
