#pragma once
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
using namespace std;
#include "../memoryriver/memoryriver.hpp"
struct String {
  static constexpr size_t MAX_LEN = 32;
  char s[MAX_LEN + 1];
  static size_t bounded_len(const char *p) {
    size_t i = 0;
    while (i < MAX_LEN && p[i] != '\0')
      ++i;
    return i;
  }
  String() { s[0] = '\0'; }

  String(const std::string &str) {
    std::memset(s, 0, sizeof(s));
    size_t len = str.size() < MAX_LEN ? str.size() : MAX_LEN;
    std::memcpy(s, str.data(), len);
    s[len] = '\0';
  }

  String &operator=(const std::string &str) {
    std::memset(s, 0, sizeof(s));
    size_t len = str.size() < MAX_LEN ? str.size() : MAX_LEN;
    std::memcpy(s, str.data(), len);
    s[len] = '\0';
    return *this;
  }

  std::string to_string() const { return std::string(s, bounded_len(s)); }

  bool operator==(const String &o) const {
    return std::strncmp(s, o.s, MAX_LEN + 1) == 0;
  }
  bool operator!=(const String &o) const { return !(*this == o); }
  bool operator<(const String &o) const {
    return std::strncmp(s, o.s, MAX_LEN + 1) < 0;
  }
  bool operator>(const String &o) const {
    return std::strncmp(s, o.s, MAX_LEN + 1) > 0;
  }
  bool operator<=(const String &o) const { return !(*this > o); }
  bool operator>=(const String &o) const { return !(*this < o); }
  size_t hash() const {
    size_t h = 1469598103934665603ull;
    for (size_t i = 0; i < MAX_LEN && s[i] != '\0'; ++i) {
      h ^= static_cast<unsigned char>(s[i]);
      h *= 1099511628211ull;
    }
    return h;
  }
  static string FromInt(int x) {
    string st = "";
    while (x) {
      st += char(x % 10 + '0');
      x /= 10;
    }
    if (st.size() == 0)
      st = "00";
    else if (st.size() == 1)
      st = "0" + st;
    else
      swap(st[0], st[1]);
    return st;
  }
  static String min_value() {
    String ms;
    ms.s[0] = '\0';
    return ms;
  }
  friend std::ostream &operator<<(std::ostream &os, const String &str) {
    os << str.s;
    return os;
  }

  void serialize(std::ostream &os) const { os.write(s, MAX_LEN + 1); }
  void deserialize(std::istream &is) {
    is.read(s, MAX_LEN + 1);
    s[MAX_LEN] = '\0';
    if (std::memchr(s, '\0', MAX_LEN + 1) == nullptr) {
      s[MAX_LEN] = '\0';
    }
  }
};

namespace std {
template <> struct hash<String> {
  size_t operator()(const String &v) const noexcept {

    size_t h = 1469598103934665603ull;
    for (size_t i = 0; i < String::MAX_LEN && v.s[i] != '\0'; ++i) {
      h ^= static_cast<unsigned char>(v.s[i]);
      h *= 1099511628211ull;
    }
    return h;
  }
};
} // namespace std