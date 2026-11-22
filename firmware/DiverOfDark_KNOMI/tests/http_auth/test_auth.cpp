// Synthetic credentials only. The real production guard is compiled unchanged.
// ESP HTTP boundaries are fakes; SHA256/Base64 use Windows system crypto.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <algorithm>
#include "HttpBasicAuth.h"
#include "nvs.h"
#include "ArduinoCompat.h"
#include "pages/ApiAuth.h"
#include "pages/WebsocketLog.h"
#include "KnomiErrorCallbacks.inc" // actual production callback bodies extracted by the test runner

extern "C" int mbedtls_base64_decode(unsigned char *out, size_t capacity, size_t *written,
                                     const unsigned char *data, size_t length) {
  DWORD n = static_cast<DWORD>(capacity);
  if (!CryptStringToBinaryA(reinterpret_cast<const char *>(data), static_cast<DWORD>(length),
                            CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT, out, &n, nullptr, nullptr)) return -1;
  *written = n;
  return 0;
}
extern "C" int mbedtls_sha256_ret(const unsigned char *data, size_t length, unsigned char *out, int) {
  BCRYPT_ALG_HANDLE alg = nullptr;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return -1;
  auto result = BCryptHash(alg, nullptr, 0, const_cast<PUCHAR>(data), static_cast<ULONG>(length), out, 32);
  BCryptCloseAlgorithmProvider(alg, 0);
  return result == 0 ? 0 : -1;
}
static std::string header(httpd_req_t *r, const char *name) {
  return std::strcmp(name, "Authorization") == 0 ? r->authorization : r->request_headers[name];
}
extern "C" size_t httpd_req_get_hdr_value_len(httpd_req_t *r, const char *name) { return header(r, name).size(); }
extern "C" esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r, const char *name, char *out, size_t n) {
  auto value = header(r, name);
  if (r->header_read_fails || n <= value.size()) return ESP_FAIL;
  std::memcpy(out, value.c_str(), value.size() + 1);
  return ESP_OK;
}
extern "C" esp_err_t httpd_resp_set_status(httpd_req_t *r, const char *s) { r->status = s; return ESP_OK; }
extern "C" esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *k, const char *v) {
  r->headers[k] = v; return ESP_OK;
}
extern "C" esp_err_t httpd_resp_sendstr(httpd_req_t *r, const char *s) { r->response = s; if (r->status.empty()) r->status = "200 OK"; return ESP_OK; }
extern "C" esp_err_t httpd_register_uri_handler(httpd_handle_t, const httpd_uri_t *r) { registered.push_back(*r); return ESP_OK; }
extern "C" esp_err_t httpd_register_err_handler(httpd_handle_t, httpd_err_code_t code, error_handler handler) { error_handlers[code] = handler; return ESP_OK; }
extern "C" int httpd_req_recv(httpd_req_t *r, char *out, size_t length) {
  auto n = std::min(length, r->body.size() - r->body_read);
  std::memcpy(out, r->body.data() + r->body_read, n); r->body_read += n; return static_cast<int>(n);
}
extern "C" esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *s) { r->headers["Content-Type"] = s; return ESP_OK; }
extern "C" esp_err_t httpd_resp_send_err(httpd_req_t *r, httpd_err_code_t code, const char *s) { r->status = code == HTTPD_405_METHOD_NOT_ALLOWED ? "405 Method Not Allowed" : "400 Bad Request"; r->response = s; return ESP_OK; }
extern "C" esp_err_t httpd_get_client_list(httpd_handle_t, size_t *n, int *) { *n = 0; return ESP_OK; }
extern "C" esp_err_t httpd_sess_trigger_close(httpd_handle_t, int) { return ESP_OK; }
extern "C" esp_err_t httpd_ws_send_frame_async(httpd_handle_t, int, httpd_ws_frame_t *) { ++ws_sends; return ESP_OK; }
extern "C" esp_err_t httpd_queue_work(httpd_handle_t, void (*fn)(void *), void *arg) { fn(arg); return ESP_OK; }
extern "C" int httpd_req_to_sockfd(httpd_req_t *) { return 7; }
extern "C" void *httpd_sess_get_ctx(httpd_handle_t h, int) { return static_cast<httpd_req_t *>(h)->sess_ctx; }
extern "C" httpd_ws_client_info_t httpd_ws_get_fd_info(httpd_handle_t h, int) {
  return static_cast<httpd_req_t *>(h)->websocket ? HTTPD_WS_CLIENT_WEBSOCKET : HTTPD_WS_CLIENT_HTTP;
}
extern "C" esp_err_t __real_httpd_ws_respond_server_handshake(httpd_req_t *r, const char *) {
  ++r->upgrades; r->websocket = true; r->status = "101 Switching Protocols"; return ESP_OK;
}
extern "C" esp_err_t __wrap_httpd_ws_respond_server_handshake(httpd_req_t *, const char *);
static esp_err_t business(httpd_req_t *r) {
  assert(r->body == "untouched upload body");
  ++r->business_calls; return 42; // guard must preserve the handler return value
}
class ProbePage : public AbstractPage {
public:
  ProbePage(http_method method, const char *path) : AbstractPage(nullptr, method, path) {}
  esp_err_t handler(httpd_req_t *r) override { return business(r); }
};
static esp_err_t route(httpd_req_t *r) {
  ProbePage page(r->method, r->uri);
  auto registeredRoute = registered.back();
  r->user_ctx = registeredRoute.user_ctx;
  return registeredRoute.handler(r);
}
static void queuedLog(httpd_req_t &r) {
  auto *arg = new WebsocketLog::async_resp_arg{r.handle, 7, 3, new char[4]{'l','o','g',0}};
  WebsocketLog::ws_async_send(arg);
}
static httpd_req_t request(const char *uri, http_method method, const std::string &auth) {
  httpd_req_t r; r.uri = uri; r.method = method; r.authorization = auth; return r;
}
static void denied(const httpd_req_t &r) {
  assert(r.status == "401 Unauthorized");
  assert(r.headers.at("WWW-Authenticate") == "Basic realm=\"KNOMI\", charset=\"UTF-8\"");
  assert(r.headers.at("Cache-Control") == "no-store");
  assert(r.headers.at("Connection") == "close");
  assert(r.business_calls == 0 && r.upgrades == 0);
  assert(r.response.find("synthetic-test-only-password") == std::string::npos);
  assert(r.response.find(r.authorization) == std::string::npos || r.authorization.empty());
}
int main() {
  const std::string good = "Basic YWRtaW46c3ludGhldGljLXRlc3Qtb25seS1wYXNzd29yZA==";
  HttpBasicAuth::begin();
  auto recycled = request("/", HTTP_GET, ""); recycled.handle = &recycled;
  queuedLog(recycled);
  assert(ws_sends == 0); // a reused HTTP fd must never receive an old queued WS log
  auto first = request("/", HTTP_GET, ""); first.handle = &first;
  assert(!HttpBasicAuth::configured());
  assert(HttpBasicAuth::dispatch(&first, business) == 42); // open bootstrap, user-approved
  auto bootstrapWs = request("/ws", HTTP_GET, ""); bootstrapWs.handle = &bootstrapWs;
  assert(__wrap_httpd_ws_respond_server_handshake(&bootstrapWs, nullptr) == ESP_OK);
  assert(!HttpBasicAuth::setPassword("short", 5));
  assert(HttpBasicAuth::setPassword("synthetic-test-only-password", 28));
  assert(HttpBasicAuth::configured());
  assert(!HttpBasicAuth::websocketAuthorized(bootstrapWs.handle, 7));
  assert(HttpBasicAuth::dispatch(&bootstrapWs, business) == ESP_FAIL);
  bootstrapWs.free_ctx(bootstrapWs.sess_ctx);
  auto persisted = FakeNvs::record;
  const std::string synthetic = "synthetic-test-only-password";
  assert(std::search(persisted.begin(), persisted.end(), synthetic.begin(), synthetic.end()) == persisted.end());
  HttpBasicAuth::begin(); // reboot retains password, never reopens bootstrap
  assert(HttpBasicAuth::configured());
  const char *paths[] = {"/", "/api/restart", "/api/scanWifi", "/api/dumpHeap", "/api/listFiles",
    "/api/status", "/api/coredump", "/api/themeConfig", "/api/uploadFile", "/api/configwifi",
    "/update", "/fs/config.json", "/fs/image.gif", "/ws", "/unknown"};
  int scenarios = 0;
  for (auto path : paths) for (auto method : {HTTP_GET, HTTP_POST, HTTP_DELETE, HTTP_HEAD}) {
    for (auto auth : {std::string(), std::string("Basic YWRtaW46d3Jvbmc=")}) {
      auto r = request(path, method, auth); r.handle = &r;
      assert(route(&r) == ESP_FAIL); denied(r); ++scenarios;
    }
    auto r = request(path, method, good); r.handle = &r;
    assert(route(&r) == 42 && r.business_calls == 1); ++scenarios;
  }
  for (auto bad : {"Basic", "Bearer abc", "Basic @@@@", "Basic Og==", "Basic YWRtaW46",
                  "Basic Z3Vlc3Q6c3ludGhldGljLXRlc3Qtb25seS1wYXNzd29yZA==",
                  "Basic YWRtaW46d3Jvbmc= extra", "Basic YWRtaW46AA==", "Basic YWRtaW46d3Jvbmc=\r\n"}) {
    auto r = request("/update", HTTP_POST, bad); r.handle = &r;
    assert(HttpBasicAuth::dispatch(&r, business) == ESP_FAIL); denied(r); ++scenarios;
  }
  auto longReq = request("/", HTTP_GET, std::string(600, 'A')); longReq.handle = &longReq;
  assert(HttpBasicAuth::dispatch(&longReq, business) == ESP_FAIL); denied(longReq); ++scenarios;
  auto readFailure = request("/", HTTP_GET, good); readFailure.handle = &readFailure; readFailure.header_read_fails = true;
  assert(HttpBasicAuth::dispatch(&readFailure, business) == ESP_FAIL); denied(readFailure); ++scenarios;
  auto lowerCase = request("/", HTTP_GET, "basic " + good.substr(6)); lowerCase.handle = &lowerCase;
  assert(HttpBasicAuth::dispatch(&lowerCase, business) == 42); ++scenarios;
  for (auto auth : {std::string(), std::string("Basic YWRtaW46d3Jvbmc="), good}) {
    auto r = request("/ws", HTTP_GET, auth); r.handle = &r;
    auto result = __wrap_httpd_ws_respond_server_handshake(&r, nullptr);
    if (auth != good) { assert(result == ESP_FAIL); denied(r); }
    else {
      assert(result == ESP_OK && r.upgrades == 1 && r.status == "101 Switching Protocols");
      r.authorization.clear(); // SDK need not retain HTTP headers after upgrade
      assert(HttpBasicAuth::dispatch(&r, business) == 42);
      FakeNvs::failWrite = true;
      assert(!HttpBasicAuth::setPassword("changed-test-password", 21));
      assert(HttpBasicAuth::websocketAuthorized(r.handle, 7));
      FakeNvs::failWrite = false;
      assert(HttpBasicAuth::setPassword("changed-test-password", 21));
      assert(!HttpBasicAuth::websocketAuthorized(r.handle, 7));
      auto old = request("/", HTTP_GET, good); old.handle = &old;
      assert(HttpBasicAuth::dispatch(&old, business) == ESP_FAIL); denied(old);
      r.free_ctx(r.sess_ctx);
    }
    ++scenarios;
  }
  FakeNvs::record = {9, 1, 2};
  HttpBasicAuth::begin();
  auto broken = request("/", HTTP_GET, ""); broken.handle = &broken;
  assert(HttpBasicAuth::dispatch(&broken, business) == ESP_FAIL); denied(broken);
  assert(!HttpBasicAuth::setPassword("reopen-must-fail", 16));
  FakeNvs::failOpen = true;
  HttpBasicAuth::begin();
  auto unavailable = request("/", HTTP_GET, ""); unavailable.handle = &unavailable;
  assert(HttpBasicAuth::dispatch(&unavailable, business) == ESP_FAIL); denied(unavailable);
  FakeNvs::failOpen = false;
  FakeNvs::readError = true;
  HttpBasicAuth::begin();
  auto readError = request("/", HTTP_GET, ""); readError.handle = &readError;
  assert(HttpBasicAuth::dispatch(&readError, business) == ESP_FAIL); denied(readError);
  FakeNvs::readError = false;
  FakeNvs::wrongType = true;
  HttpBasicAuth::begin();
  auto wrongType = request("/", HTTP_GET, ""); wrongType.handle = &wrongType;
  assert(HttpBasicAuth::dispatch(&wrongType, business) == ESP_FAIL); denied(wrongType);
  FakeNvs::wrongType = false;
  FakeNvs::record.clear();
  FakeNvs::namespaceExists = true;
  HttpBasicAuth::begin();
  auto missingRecord = request("/", HTTP_GET, ""); missingRecord.handle = &missingRecord;
  assert(HttpBasicAuth::dispatch(&missingRecord, business) == ESP_FAIL); denied(missingRecord);
  FakeNvs::namespaceExists = false;
  HttpBasicAuth::begin();
  ApiAuthStatusGet statusPage(nullptr); auto statusRoute = registered.back();
  auto status = request("/api/auth/status", HTTP_GET, ""); status.handle = &status; status.user_ctx = statusRoute.user_ctx;
  assert(statusRoute.handler(&status) == ESP_OK);
  assert(status.response == "{\"configured\":false,\"username\":\"admin\"}");
  ApiAuthPasswordPost passwordPage(nullptr); auto passwordRoute = registered.back();
  auto save = request("/api/auth/password", HTTP_POST, ""); save.handle = &save; save.user_ctx = passwordRoute.user_ctx;
  save.body = "{\"password\":\"synthetic-test-only-password\"}"; save.content_len = save.body.size();
  save.request_headers["Content-Type"] = "text/plain";
  assert(passwordRoute.handler(&save) == ESP_FAIL && !HttpBasicAuth::configured());
  assert(save.body_read == 0); // reject cross-origin simple-form/content types before reading password
  save.status.clear(); save.response.clear(); save.request_headers["Content-Type"] = "application/json";
  assert(passwordRoute.handler(&save) == ESP_FAIL && HttpBasicAuth::configured());
  assert(save.status == "200 OK" && save.response == "{\"ok\":true,\"username\":\"admin\"}");
  auto unauthorizedChange = request("/api/auth/password", HTTP_POST, "");
  unauthorizedChange.handle = &unauthorizedChange; unauthorizedChange.user_ctx = passwordRoute.user_ctx;
  assert(passwordRoute.handler(&unauthorizedChange) == ESP_FAIL); denied(unauthorizedChange);
  assert(unauthorizedChange.body_read == 0);
  auto change = request("/api/auth/password", HTTP_POST, good);
  change.handle = &change; change.user_ctx = passwordRoute.user_ctx;
  change.body = "{\"password\":\"changed-test-password\"}"; change.content_len = change.body.size();
  change.request_headers["Content-Type"] = "application/json";
  assert(passwordRoute.handler(&change) == ESP_FAIL && change.status == "200 OK");
  HttpBasicAuth::begin();
  auto changed = request("/", HTTP_GET, "Basic YWRtaW46Y2hhbmdlZC10ZXN0LXBhc3N3b3Jk"); changed.handle = &changed;
  assert(route(&changed) == 42);
  auto previous = request("/", HTTP_GET, good); previous.handle = &previous;
  assert(route(&previous) == ESP_FAIL); denied(previous);
  KnomiErrorCallbacks errors;
  errors.registerNotFound();
  for (auto code : {HTTPD_404_NOT_FOUND, HTTPD_405_METHOD_NOT_ALLOWED}) {
    auto anonymous = request("/unknown", HTTP_HEAD, ""); anonymous.handle = &anonymous;
    assert(error_handlers.at(code)(&anonymous, code) == ESP_FAIL); denied(anonymous);
    auto allowed = request("/unknown", HTTP_HEAD, changed.authorization); allowed.handle = &allowed;
    assert(error_handlers.at(code)(&allowed, code) == ESP_OK);
    assert(allowed.status == (code == HTTPD_404_NOT_FOUND ? "303 See Other" : "405 Method Not Allowed"));
  }
  std::cout << "PASS: " << scenarios << " native route/header/WS cases + bootstrap/NVS/API/error/session assertions\n";
}
