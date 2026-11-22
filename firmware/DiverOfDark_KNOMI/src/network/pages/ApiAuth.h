#pragma once
#include "AbstractPage.h"
#include <ArduinoJson.h>
#include <cstring>

class ApiAuthStatusGet : public AbstractPage {
public:
  explicit ApiAuthStatusGet(httpd_handle_t server) : AbstractPage(server, HTTP_GET, "/api/auth/status") {}
  esp_err_t handler(httpd_req_t *request) override {
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, HttpBasicAuth::configured() ?
      "{\"configured\":true,\"username\":\"admin\"}" : "{\"configured\":false,\"username\":\"admin\"}");
  }
};

class ApiAuthPasswordPost : public AbstractPage {
public:
  explicit ApiAuthPasswordPost(httpd_handle_t server) : AbstractPage(server, HTTP_POST, "/api/auth/password") {}
  esp_err_t handler(httpd_req_t *request) override {
    char contentType[64] = {};
    // application/json prevents cross-origin HTML forms from changing bootstrap credentials.
    // No CORS enablement or URL/form/query-string passwords.
    if (httpd_req_get_hdr_value_str(request, "Content-Type", contentType, sizeof(contentType)) != ESP_OK ||
        (std::strcmp(contentType, "application/json") && std::strcmp(contentType, "application/json; charset=UTF-8")) ||
        request->content_len == 0 || request->content_len >= 512) {
      httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Expected bounded application/json password body");
      return ESP_FAIL;
    }
    char body[512] = {};
    size_t received = 0;
    while (received < request->content_len) {
      int count = httpd_req_recv(request, body + received, request->content_len - received);
      if (count <= 0) break;
      received += count;
    }
    JsonDocument document;
    bool valid = received == request->content_len && !deserializeJson(document, body) &&
                 document.is<JsonObject>() && document.size() == 1 && document["password"].is<const char *>();
    bool saved = false;
    if (valid) {
      JsonString password = document["password"].as<JsonString>();
      saved = HttpBasicAuth::setPassword(password.c_str(), password.size());
    }
    volatile char *sensitive = body;
    for (size_t i = 0; i < sizeof(body); ++i) sensitive[i] = 0;
    if (!saved) {
      httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Password must be 8-128 UTF-8 bytes; saving must succeed");
      return ESP_FAIL;
    }
    // Invalidate bootstrap/old-password sockets immediately (generation check),
    // then close them. Queued log callbacks also re-check the generation before sending.
    int sockets[CONFIG_LWIP_MAX_LISTENING_TCP] = {};
    size_t count = CONFIG_LWIP_MAX_LISTENING_TCP;
    if (httpd_get_client_list(request->handle, &count, sockets) == ESP_OK) {
      for (size_t i = 0; i < count; ++i)
        if (httpd_ws_get_fd_info(request->handle, sockets[i]) == HTTPD_WS_CLIENT_WEBSOCKET &&
            !HttpBasicAuth::websocketAuthorized(request->handle, sockets[i]))
          httpd_sess_trigger_close(request->handle, sockets[i]);
    }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "Connection", "close");
    httpd_resp_sendstr(request, "{\"ok\":true,\"username\":\"admin\"}");
    return ESP_FAIL; // close only this HTTP connection after the successful response, not the device
  }
};
