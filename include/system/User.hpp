#pragma once
#include "../container/String.hpp"
#include "../memoryriver/memoryriver.hpp"
#include "Ticket.hpp"
using namespace std;
namespace sjtu {
class User {
private:
  int privilege;
  String UserName, PassWord, MailAdr;
  String name;
  int bought_cnt = 0;
  bool logged_in = false;
  friend class UserSystem;
  friend class System;
  friend class Ticket;

public:
  User() : privilege(-1), bought_cnt(0), logged_in(false) {}
  User(int privilege, String UserName, String PassWord, String name,
       String MailAdr)
      : privilege(privilege), PassWord(PassWord), UserName(UserName),
        MailAdr(MailAdr), name(name) {}
  User(const User &other)
      : privilege(other.privilege), UserName(other.UserName),
        PassWord(other.PassWord), name(other.name), MailAdr(other.MailAdr),
        bought_cnt(other.bought_cnt), logged_in(other.logged_in) {}
  ~User() {}
  User &operator=(const User &other) {
    if (this == &other)
      return *this;
    privilege = other.privilege;
    UserName = other.UserName, PassWord = other.PassWord;
    MailAdr = other.MailAdr;
    bought_cnt = other.bought_cnt;
    logged_in = other.logged_in;
    name = other.name;
    return *this;
  }
  bool operator==(const User &other) const {
    return UserName == other.UserName;
  }
  bool operator!=(const User &other) const {
    return UserName != other.UserName;
  }
  bool operator<(const User &other) const { return UserName < other.UserName; }
  bool operator<=(const User &other) const {
    return UserName <= other.UserName;
  }
  bool operator>(const User &other) const { return !(*this <= other); }
  bool operator>=(const User &other) const { return !(*this < other); }

  void serialize(std::ostream &os) const {
    os.write(reinterpret_cast<const char *>(&privilege), sizeof(privilege));
    Serializer<String>::write(os, UserName);
    Serializer<String>::write(os, PassWord);
    Serializer<String>::write(os, MailAdr);
    Serializer<String>::write(os, name);
    os.write(reinterpret_cast<const char *>(&bought_cnt), sizeof(bought_cnt));
    os.write(reinterpret_cast<const char *>(&logged_in), sizeof(logged_in));
  }
  void deserialize(std::istream &is) {
    is.read(reinterpret_cast<char *>(&privilege), sizeof(privilege));
    Serializer<String>::read(is, UserName);
    Serializer<String>::read(is, PassWord);
    Serializer<String>::read(is, MailAdr);
    Serializer<String>::read(is, name);
    is.read(reinterpret_cast<char *>(&bought_cnt), sizeof(bought_cnt));
    if (!is.good() || bought_cnt < 0) {
      bought_cnt = 0;
      is.clear();
    }
    is.read(reinterpret_cast<char *>(&logged_in), sizeof(logged_in));
  }
};

} // namespace sjtu