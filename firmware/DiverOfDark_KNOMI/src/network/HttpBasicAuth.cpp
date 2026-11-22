#include "HttpBasicAuth.h"
#include <nvs.h>
#include <esp_system.h>
#include <esp_idf_version.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>
#include <cstring>
#include <cstdlib>

#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(4, 4, 7)
#error "Revalidate the HTTP Basic pre-WebSocket handshake wrapper before changing ESP-IDF."
#endif

namespace {
constexpr size_t maxHeader = 512;
constexpr size_t maxCredential = 192;
enum class State { Locked, Bootstrap, Ready };
State state = State::Locked;
// One atomic NVS blob: schema byte, random 16-byte salt, 32-byte verifier.
unsigned char record[49] = {};
unsigned long generation = 0;
constexpr unsigned long sessionMagic = 0x4b4e4f4d;
struct Session { unsigned long magic; unsigned long generation; };

bool validSession(void *context) {
  auto *session = static_cast<Session *>(context);
  return session && session->magic == sessionMagic && session->generation == generation;
}

void erase(void *data, size_t length) {
  volatile unsigned char *p = static_cast<volatile unsigned char *>(data);
  while (length--) *p++ = 0;
}

bool checkHeader(const char *header, size_t length) {
  // Auth schemes are case-insensitive; credential contents are not.
  if (length < 10 || (header[0] | 32) != 'b' || (header[1] | 32) != 'a' ||
      (header[2] | 32) != 's' || (header[3] | 32) != 'i' || (header[4] | 32) != 'c' || header[5] != ' ')
    return false;
  const char *token = header + 6;
  const size_t tokenLength = length - 6;
  if (tokenLength % 4) return false;
  size_t padding = 0;
  for (size_t i = 0; i < tokenLength; ++i) {
    unsigned char c = static_cast<unsigned char>(token[i]);
    if (c == '=') {
      if (++padding > 2 || i < tokenLength - 2) return false;
    } else if (padding || !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                           (c >= '0' && c <= '9') || c == '+' || c == '/')) return false;
  }
  unsigned char decoded[maxCredential] = {};
  size_t decodedLength = 0;
  bool valid = mbedtls_base64_decode(decoded, sizeof(decoded), &decodedLength,
                                    reinterpret_cast<const unsigned char *>(token), tokenLength) == 0;
  valid = valid && decodedLength > 6 && std::memcmp(decoded, "admin:", 6) == 0;
  for (size_t i = 0; valid && i < decodedLength; ++i)
    if (decoded[i] < 0x20 || decoded[i] == 0x7f) valid = false;

  unsigned char input[16 + maxCredential] = {};
  unsigned char actual[32] = {};
  if (valid) {
    std::memcpy(input, record + 1, 16);
    std::memcpy(input + 16, decoded, decodedLength);
    valid = mbedtls_sha256_ret(input, 16 + decodedLength, actual, 0) == 0;
    unsigned char difference = 0;
    for (size_t i = 0; i < sizeof(actual); ++i) difference |= actual[i] ^ record[17 + i];
    valid = valid && difference == 0;
  }
  erase(input, sizeof(input));
  erase(actual, sizeof(actual));
  erase(decoded, sizeof(decoded));
  return valid;
}
}

void HttpBasicAuth::begin() {
  state = State::Locked; // an NVS failure/corrupt record must never reopen first-use access
  ++generation;
  erase(record, sizeof(record));
  nvs_handle_t handle;
  esp_err_t opened = nvs_open("knomi_auth", NVS_READONLY, &handle);
  if (opened == ESP_ERR_NVS_NOT_FOUND) { state = State::Bootstrap; return; }
  if (opened != ESP_OK) return;
  size_t length = 0;
  // Once the namespace exists, a missing/wrong-type/unreadable record is NOT first use.
  if (nvs_get_blob(handle, "record", nullptr, &length) == ESP_OK && length == sizeof(record) &&
      nvs_get_blob(handle, "record", record, &length) == ESP_OK && length == sizeof(record) && record[0] == 1) {
    unsigned char saltNonzero = 0, digestNonzero = 0;
    for (size_t i = 1; i < 17; ++i) saltNonzero |= record[i];
    for (size_t i = 17; i < sizeof(record); ++i) digestNonzero |= record[i];
    if (saltNonzero && digestNonzero) state = State::Ready;
  }
  nvs_close(handle);
}

bool HttpBasicAuth::configured() { return state == State::Ready; }

bool HttpBasicAuth::setPassword(const char *password, size_t length) {
  if (state == State::Locked || !password || length < 8 || length > 128) return false;
  for (size_t i = 0; i < length; ++i)
    if (static_cast<unsigned char>(password[i]) < 0x20 || password[i] == 0x7f) return false;
  unsigned char next[49] = {1};
  esp_fill_random(next + 1, 16);
  unsigned char input[16 + 6 + 128] = {};
  std::memcpy(input, next + 1, 16);
  std::memcpy(input + 16, "admin:", 6);
  std::memcpy(input + 22, password, length);
  bool saved = mbedtls_sha256_ret(input, 22 + length, next + 17, 0) == 0;
  erase(input, sizeof(input));
  nvs_handle_t handle;
  if (saved && nvs_open("knomi_auth", NVS_READWRITE, &handle) == ESP_OK) {
    saved = nvs_set_blob(handle, "record", next, sizeof(next)) == ESP_OK && nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
  } else saved = false;
  if (saved) {
    std::memcpy(record, next, sizeof(record));
    state = State::Ready;
    ++generation;
  }
  erase(next, sizeof(next));
  return saved;
}

bool HttpBasicAuth::websocketAuthorized(httpd_handle_t server, int fd) {
  return httpd_ws_get_fd_info(server, fd) == HTTPD_WS_CLIENT_WEBSOCKET &&
         validSession(httpd_sess_get_ctx(server, fd));
}

bool HttpBasicAuth::authorize(httpd_req_t *request) {
  if (state == State::Bootstrap) return true; // explicit first-use product requirement
  char header[maxHeader + 1] = {};
  size_t length = httpd_req_get_hdr_value_len(request, "Authorization");
  bool valid = state == State::Ready && length > 0 && length <= maxHeader &&
               httpd_req_get_hdr_value_str(request, "Authorization", header, sizeof(header)) == ESP_OK &&
               checkHeader(header, length);
  erase(header, sizeof(header));
  if (valid) return true;
  httpd_resp_set_status(request, "401 Unauthorized");
  httpd_resp_set_hdr(request, "WWW-Authenticate", "Basic realm=\"KNOMI\", charset=\"UTF-8\"");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  httpd_resp_set_hdr(request, "Connection", "close");
  httpd_resp_sendstr(request, "Authentication required");
  return false;
}

esp_err_t HttpBasicAuth::dispatch(httpd_req_t *request, esp_err_t (*handler)(httpd_req_t *)) {
  // Upgraded sockets have already passed the guarded SDK handshake below.
  // Their HTTP headers may no longer exist. Never treat an ordinary HTTP keep-alive as a login session.
  if (httpd_ws_get_fd_info(request->handle, httpd_req_to_sockfd(request)) == HTTPD_WS_CLIENT_WEBSOCKET) {
    if (!validSession(request->sess_ctx)) return ESP_FAIL;
  } else if (!authorize(request)) return ESP_FAIL;
  return handler(request);
}

// ESP-IDF 4.4.7 calls this BEFORE the URI callback; a callback-only guard is too late.
// -Wl,--wrap=httpd_ws_respond_server_handshake is required and checked by the build script.
extern "C" esp_err_t __real_httpd_ws_respond_server_handshake(httpd_req_t *, const char *);
extern "C" esp_err_t __wrap_httpd_ws_respond_server_handshake(httpd_req_t *request, const char *subprotocol) {
  if (!HttpBasicAuth::authorize(request)) return ESP_FAIL;
  auto *session = static_cast<Session *>(std::malloc(sizeof(Session)));
  if (!session) return ESP_FAIL;
  *session = {sessionMagic, generation};
  esp_err_t result = __real_httpd_ws_respond_server_handshake(request, subprotocol);
  if (result != ESP_OK) std::free(session);
  else {
    request->sess_ctx = session;
    request->free_ctx = std::free;
  }
  return result;
}
