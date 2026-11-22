#pragma once
#include <stddef.h>
#include <string>
#include <map>
#include <vector>
#define CONFIG_LWIP_MAX_LISTENING_TCP 8
typedef int esp_err_t;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
typedef void *httpd_handle_t;
enum http_method { HTTP_GET, HTTP_POST, HTTP_DELETE, HTTP_HEAD };
enum httpd_ws_client_info_t { HTTPD_WS_CLIENT_HTTP, HTTPD_WS_CLIENT_WEBSOCKET };
struct httpd_req_t {
  httpd_handle_t handle = nullptr;
  http_method method = HTTP_GET;
  const char *uri = "/";
  void *user_ctx = nullptr;
  void *sess_ctx = nullptr;
  void (*free_ctx)(void *) = nullptr;
  std::string authorization;
  std::string status;
  std::string response;
  std::map<std::string, std::string> headers;
  bool websocket = false;
  bool header_read_fails = false;
  int business_calls = 0;
  int upgrades = 0;
  std::string body = "untouched upload body";
  size_t content_len = 0;
  size_t body_read = 0;
  std::map<std::string, std::string> request_headers;
};
typedef esp_err_t (*httpd_uri_func)(httpd_req_t *);
struct httpd_uri_t {
  const char *uri;
  http_method method;
  httpd_uri_func handler;
  void *user_ctx;
  bool is_websocket;
};
enum httpd_err_code_t { HTTPD_400_BAD_REQUEST, HTTPD_404_NOT_FOUND, HTTPD_405_METHOD_NOT_ALLOWED, HTTPD_500_INTERNAL_SERVER_ERROR };
using error_handler = esp_err_t (*)(httpd_req_t *, httpd_err_code_t);
inline std::map<httpd_err_code_t, error_handler> error_handlers;
enum { HTTPD_WS_TYPE_TEXT };
struct httpd_ws_frame_t { int type; size_t len; unsigned char *payload; };
inline std::vector<httpd_uri_t> registered;
inline int ws_sends = 0;
inline const char *http_method_str(http_method) { return "TEST"; }
inline const char *esp_err_to_name(esp_err_t) { return "TEST"; }
extern "C" {
size_t httpd_req_get_hdr_value_len(httpd_req_t *, const char *);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *, const char *, char *, size_t);
esp_err_t httpd_resp_set_status(httpd_req_t *, const char *);
esp_err_t httpd_resp_set_hdr(httpd_req_t *, const char *, const char *);
esp_err_t httpd_resp_sendstr(httpd_req_t *, const char *);
int httpd_req_to_sockfd(httpd_req_t *);
httpd_ws_client_info_t httpd_ws_get_fd_info(httpd_handle_t, int);
void *httpd_sess_get_ctx(httpd_handle_t, int);
esp_err_t httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t *);
esp_err_t httpd_register_err_handler(httpd_handle_t, httpd_err_code_t, error_handler);
int httpd_req_recv(httpd_req_t *, char *, size_t);
esp_err_t httpd_resp_set_type(httpd_req_t *, const char *);
esp_err_t httpd_resp_send_err(httpd_req_t *, httpd_err_code_t, const char *);
esp_err_t httpd_get_client_list(httpd_handle_t, size_t *, int *);
esp_err_t httpd_sess_trigger_close(httpd_handle_t, int);
esp_err_t httpd_ws_send_frame_async(httpd_handle_t, int, httpd_ws_frame_t *);
esp_err_t httpd_queue_work(httpd_handle_t, void (*)(void *), void *);
}
inline esp_err_t httpd_resp_send_500(httpd_req_t *r) { return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Error"); }
