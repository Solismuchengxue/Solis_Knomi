#pragma once
#include <algorithm>
#include <functional>
#include <string>
#include <cstring>
using uint = unsigned;
using std::min;
#define LV_LOG_DEBUG(...) ((void)0)
#define LV_LOG_INFO(...) ((void)0)
#define LV_LOG_WARN(...) ((void)0)
class String : public std::string {
public:
  String() = default;
  String(const char *s) : std::string(s) {}
  String(const std::string &s) : std::string(s) {}
  bool equals(const char *s) const { return *this == s; }
  bool isEmpty() const { return empty(); }
  bool startsWith(const char *s) const { return find(s) == 0; }
  int indexOf(const char *s) const { auto p = find(s); return p == npos ? -1 : static_cast<int>(p); }
  String substring(size_t start) const { return substr(start); }
  void concat(const char *s, size_t n) { append(s, n); }
  friend String operator+(const char *s, const String &v) { return String(std::string(s) + v.c_str()); }
};
