#pragma once
#include <stddef.h>
inline void esp_fill_random(void *buffer, size_t n) {
  auto p = static_cast<unsigned char *>(buffer);
  for (size_t i = 0; i < n; ++i) p[i] = static_cast<unsigned char>(i + 1);
}
