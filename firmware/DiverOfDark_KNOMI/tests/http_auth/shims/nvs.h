#pragma once
#include "esp_http_server.h"
#include <cstring>
#include <vector>
using nvs_handle_t = unsigned;
enum nvs_open_mode_t { NVS_READONLY, NVS_READWRITE };
constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND = 0x1102;
constexpr esp_err_t ESP_ERR_NVS_TYPE_MISMATCH = 0x1103;
namespace FakeNvs {
inline std::vector<unsigned char> record, pending;
inline bool namespaceExists = false;
inline bool failOpen = false, failWrite = false, readError = false, wrongType = false;
}
inline esp_err_t nvs_open(const char *, nvs_open_mode_t mode, nvs_handle_t *handle) {
  if (FakeNvs::failOpen) return ESP_FAIL;
  if (mode == NVS_READONLY && !FakeNvs::namespaceExists && FakeNvs::record.empty()) return ESP_ERR_NVS_NOT_FOUND;
  if (mode == NVS_READWRITE) FakeNvs::namespaceExists = true;
  *handle = 1; return ESP_OK;
}
inline esp_err_t nvs_get_blob(nvs_handle_t, const char *, void *out, size_t *length) {
  if (FakeNvs::readError) return ESP_FAIL;
  if (FakeNvs::wrongType) return ESP_ERR_NVS_TYPE_MISMATCH;
  if (FakeNvs::record.empty()) return ESP_ERR_NVS_NOT_FOUND;
  if (out) {
    if (*length < FakeNvs::record.size()) return ESP_FAIL;
    std::memcpy(out, FakeNvs::record.data(), FakeNvs::record.size());
  }
  *length = FakeNvs::record.size(); return ESP_OK;
}
inline esp_err_t nvs_set_blob(nvs_handle_t, const char *, const void *data, size_t length) {
  if (FakeNvs::failWrite) return ESP_FAIL;
  auto p = static_cast<const unsigned char *>(data);
  FakeNvs::pending.assign(p, p + length); return ESP_OK;
}
inline esp_err_t nvs_commit(nvs_handle_t) { FakeNvs::record = FakeNvs::pending; return ESP_OK; }
inline void nvs_close(nvs_handle_t) { FakeNvs::pending.clear(); }
