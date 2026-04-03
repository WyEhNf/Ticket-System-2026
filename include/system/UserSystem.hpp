#pragma once
#include "../container/bpt.hpp"
#include "../container/hashmap.hpp"
#include "User.hpp"
using namespace std;
namespace sjtu {
struct UserOrderKey {
  String user_id;
  int seq = 0;

  bool operator==(const UserOrderKey &o) const {
    return user_id == o.user_id && seq == o.seq;
  }
  bool operator!=(const UserOrderKey &o) const { return !(*this == o); }
  bool operator<(const UserOrderKey &o) const {
    if (user_id != o.user_id)
      return user_id < o.user_id;
    return seq < o.seq;
  }
  bool operator<=(const UserOrderKey &o) const { return !(o < *this); }
  bool operator>(const UserOrderKey &o) const { return o < *this; }
  bool operator>=(const UserOrderKey &o) const { return !(*this < o); }

  std::size_t hash() const {
    std::size_t h1 = user_id.hash();
    std::size_t h2 = hash_mix64(static_cast<std::size_t>(seq));
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
  }

  void serialize(std::ostream &os) const {
    Serializer<String>::write(os, user_id);
    os.write(reinterpret_cast<const char *>(&seq), sizeof(seq));
  }
  void deserialize(std::istream &is) {
    Serializer<String>::read(is, user_id);
    is.read(reinterpret_cast<char *>(&seq), sizeof(seq));
  }
};

} // namespace sjtu

namespace sjtu {

class UserSystem {
private:
  struct UserBrief {
    int privilege = 0;
    bool logged_in = false;
  };
  BPlusTree<String, int, 8> user_tree;
  BPlusTree<UserOrderKey, int, 8> order_tree;
  MemoryRiver<User> user_store;
  MemoryRiver<order> order_store;
  hashmap<String, int> user_pos_cache;
  hashmap<String, User> user_obj_cache;
  hashmap<String, UserBrief> user_brief_cache;
  hashmap<UserOrderKey, int> order_pos_cache;
  static constexpr int USER_CACHE_LIMIT = 16;
  static constexpr int ORDER_POS_CACHE_LIMIT = 128;
  friend class System;

public:
  UserSystem(string filename = "user_tree.data")
      : user_tree(filename + ".uidx", 4), order_tree(filename + ".oidx", 4),
        user_store(filename + ".ublob"), order_store(filename + ".oblob") {
    user_store.initialise(filename + ".ublob");
    order_store.initialise(filename + ".oblob");
  }
  ~UserSystem() {}
  bool add_user(const User &new_user);
  bool delete_user(String user_id);
  bool find_user(String user_id, User &out_user);
  bool add_ticket(String user_id, const Ticket &ticket, int num,
                  OrderStatus status, order &out_order);
  bool add_ticket_loaded(const User &loaded_user, const Ticket &ticket, int num,
                         OrderStatus status, order &out_order);
  bool refund_ticket(String user_id, int pos, order &out_order);
  bool query_ordered_tickets(const String &user_id);
  bool login(String user_id, String password);
  bool logout(String user_id);
  bool modify_user(String user_id, const User &new_user);
  bool get_user_pos(const String &user_id, int &out_pos);
  bool get_user_at(int user_pos, User &out_user);
  bool get_user_brief(const String &user_id, int &privilege, bool &logged_in);
  void modify_order(order &o, OrderStatus new_status);
  bool get_order(String user_id, int seq, order &out_order);
  void clean_up();
  void flush_all();
};
} // namespace sjtu