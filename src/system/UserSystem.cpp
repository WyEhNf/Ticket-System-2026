#include "../../include/system/UserSystem.hpp"
#include "../../include/container/map.hpp"
#include "../../include/system/TicketSystem.hpp"
using namespace std;
namespace sjtu {

namespace {
template <typename MapT> void trim_small_cache(MapT &cache, size_t limit) {
  while (cache.size() > limit)
    cache.erase(cache.begin());
}
} // namespace

static bool load_user_pos(hashmap<String, int> &user_pos_cache,
                          BPlusTree<String, int, 8> &user_tree,
                          const String &user_id, int &user_pos) {
  auto it = user_pos_cache.find(user_id);
  if (it != user_pos_cache.end()) {
    user_pos = it->second;
    return true;
  }
  if (!user_tree.find_first_value(user_id, user_pos))
    return false;
  user_pos_cache[user_id] = user_pos;
  return true;
}

static bool load_order_pos(hashmap<UserOrderKey, int> &order_pos_cache,
                           BPlusTree<UserOrderKey, int, 8> &order_tree,
                           const UserOrderKey &key, int &order_pos) {
  auto it = order_pos_cache.find(key);
  if (it != order_pos_cache.end()) {
    order_pos = it->second;
    return true;
  }
  if (!order_tree.find_first_value(key, order_pos))
    return false;
  order_pos_cache[key] = order_pos;
  return true;
}

bool UserSystem::add_user(const User &new_user) {
  if (user_tree.contains_index(new_user.UserName))
    return false;
  int user_pos = user_store.write(new_user);
  user_tree.insert(new_user.UserName, user_pos);
  user_pos_cache[new_user.UserName] = user_pos;
  user_obj_cache[new_user.UserName] = std::move(const_cast<User &>(new_user));
  user_brief_cache[new_user.UserName] =
      UserBrief{new_user.privilege, new_user.logged_in};
  trim_small_cache(user_pos_cache, USER_CACHE_LIMIT);
  trim_small_cache(user_obj_cache, USER_CACHE_LIMIT);
  trim_small_cache(user_brief_cache, USER_CACHE_LIMIT);
  return true;
}
bool UserSystem::delete_user(String user_id) {
  int user_pos = 0;
  if (!load_user_pos(user_pos_cache, user_tree, user_id, user_pos))
    return false;
  user_tree.erase(user_id, user_pos);
  user_pos_cache.erase(user_id);
  user_obj_cache.erase(user_id);
  user_brief_cache.erase(user_id);
  return true;
}
bool UserSystem::login(String user_id, String password) {
  int user_pos = 0;
  if (!load_user_pos(user_pos_cache, user_tree, user_id, user_pos))
    return false;
  User u;
  auto cache_it = user_obj_cache.find(user_id);
  if (cache_it != user_obj_cache.end())
    u = cache_it->second;
  else
    user_store.read(u, user_pos);
  if (u.PassWord != password)
    return false;
  if (u.logged_in)
    return false;
  u.logged_in = true;
  user_store.update(u, user_pos);
  user_obj_cache[user_id] = std::move(u);
  user_brief_cache[user_id] = UserBrief{u.privilege, u.logged_in};
  trim_small_cache(user_obj_cache, USER_CACHE_LIMIT);
  trim_small_cache(user_brief_cache, USER_CACHE_LIMIT);
  return true;
}
bool UserSystem::logout(String user_id) {
  int user_pos = 0;
  if (!load_user_pos(user_pos_cache, user_tree, user_id, user_pos))
    return false;
  User u;
  auto cache_it = user_obj_cache.find(user_id);
  if (cache_it != user_obj_cache.end())
    u = cache_it->second;
  else
    user_store.read(u, user_pos);
  if (!u.logged_in)
    return false;
  u.logged_in = false;
  user_store.update(u, user_pos);
  user_obj_cache[user_id] = std::move(u);
  user_brief_cache[user_id] = UserBrief{u.privilege, u.logged_in};
  trim_small_cache(user_obj_cache, USER_CACHE_LIMIT);
  trim_small_cache(user_brief_cache, USER_CACHE_LIMIT);
  return true;
}
bool UserSystem::find_user(String user_id, User &out_user) {
  int user_pos = 0;
  if (!load_user_pos(user_pos_cache, user_tree, user_id, user_pos))
    return false;
  auto cache_it = user_obj_cache.find(user_id);
  if (cache_it != user_obj_cache.end()) {
    out_user = cache_it->second;
    return true;
  }
  User u;
  user_store.read(u, user_pos);
  user_obj_cache[user_id] = std::move(u);
  trim_small_cache(user_obj_cache, USER_CACHE_LIMIT);
  out_user = u;
  return true;
}
bool UserSystem::modify_user(String user_id, const User &new_user) {
  int user_pos = 0;
  if (!load_user_pos(user_pos_cache, user_tree, user_id, user_pos))
    return false;
  user_store.update(new_user, user_pos);
  user_obj_cache[user_id] = std::move(const_cast<User &>(new_user));
  user_brief_cache[user_id] = UserBrief{new_user.privilege, new_user.logged_in};
  trim_small_cache(user_obj_cache, USER_CACHE_LIMIT);
  trim_small_cache(user_brief_cache, USER_CACHE_LIMIT);
  return true;
}

bool UserSystem::get_user_pos(const String &user_id, int &out_pos) {
  return load_user_pos(user_pos_cache, user_tree, user_id, out_pos);
}

bool UserSystem::get_user_at(int user_pos, User &out_user) {
  if (user_pos < 0)
    return false;
  user_store.read(out_user, user_pos);
  if (out_user == User())
    return false;
  user_pos_cache[out_user.UserName] = user_pos;
  user_obj_cache[out_user.UserName] = out_user;
  user_brief_cache[out_user.UserName] =
      UserBrief{out_user.privilege, out_user.logged_in};
  trim_small_cache(user_pos_cache, USER_CACHE_LIMIT);
  trim_small_cache(user_obj_cache, USER_CACHE_LIMIT);
  trim_small_cache(user_brief_cache, USER_CACHE_LIMIT);
  return true;
}

bool UserSystem::get_user_brief(const String &user_id, int &privilege,
                                bool &logged_in) {
  auto brief_it = user_brief_cache.find(user_id);
  if (brief_it != user_brief_cache.end()) {
    privilege = brief_it->second.privilege;
    logged_in = brief_it->second.logged_in;
    return true;
  }
  int user_pos = 0;
  if (!load_user_pos(user_pos_cache, user_tree, user_id, user_pos))
    return false;
  User u;
  user_store.read(u, user_pos);
  privilege = u.privilege;
  logged_in = u.logged_in;
  user_brief_cache[user_id] = UserBrief{u.privilege, u.logged_in};
  trim_small_cache(user_brief_cache, USER_CACHE_LIMIT);
  return true;
}
bool UserSystem::query_ordered_tickets(const String &user_id) {
  int user_pos = 0;
  if (!load_user_pos(user_pos_cache, user_tree, user_id, user_pos))
    return false;
  User u;
  auto cache_it = user_obj_cache.find(user_id);
  if (cache_it != user_obj_cache.end())
    u = cache_it->second;
  else
    user_store.read(u, user_pos);
  if (!u.logged_in)
    return false;

  struct OrderTrainCache {
    Train train;
    map<String, int> station_pos;
  };
  map<String, OrderTrainCache> train_cache;

  cout << u.bought_cnt << '\n';
  for (int i = u.bought_cnt - 1; i >= 0; i--) {
    order t;
    int order_pos = 0;
    UserOrderKey okey{user_id, i};
    if (!load_order_pos(order_pos_cache, order_tree, okey, order_pos))
      continue;
    order_store.read(t, order_pos);
    cout << "[" << status_cstr(t.status) << "] ";

    auto it = train_cache.find(t.ticket.trainID);
    if (it == train_cache.end()) {
      OrderTrainCache cache;
      if (!Ticket::ptr->find_train(t.ticket.trainID, cache.train)) {
        cache.train = Train();
      }
      for (int j = 0; j < cache.train.stationNum; ++j) {
        if (cache.station_pos.find(cache.train.stations[j]) ==
            cache.station_pos.end()) {
          cache.station_pos[cache.train.stations[j]] = j;
        }
      }
      train_cache[t.ticket.trainID] = std::move(cache);
      it = train_cache.find(t.ticket.trainID);
    }

    Train &tr = it->second.train;
    auto from_it = it->second.station_pos.find(t.ticket.from_station);
    auto to_it = it->second.station_pos.find(t.ticket.to_station);
    if (from_it == it->second.station_pos.end() ||
        to_it == it->second.station_pos.end() ||
        from_it->second >= to_it->second) {
      t.ticket.printTicket(t.ticket.from_station, t.ticket.to_station, t.num);
      continue;
    }

    int total_price = 0;
    for (int j = from_it->second; j < to_it->second; ++j) {
      total_price += tr.prices[j];
    }

    cout << tr.ID << ' ' << t.ticket.from_station << ' '
         << tr.getTime(t.ticket.from_station, t.ticket.date) << " -> "
         << t.ticket.to_station << ' '
         << tr.getTime(t.ticket.to_station, t.ticket.date, 0) << ' '
         << total_price << ' ' << t.num << '\n';
  }
  return true;
}
bool UserSystem::add_ticket(String user_id, const Ticket &ticket, int num,
                            OrderStatus status, order &out_order) {
  int user_pos = 0;
  if (!load_user_pos(user_pos_cache, user_tree, user_id, user_pos))
    return false;
  User u;
  auto cache_it = user_obj_cache.find(user_id);
  if (cache_it != user_obj_cache.end())
    u = cache_it->second;
  else
    user_store.read(u, user_pos);
  order new_order{ticket, num, user_id, status};
  new_order.pos = u.bought_cnt;
  int order_pos = order_store.write(new_order);
  UserOrderKey okey{user_id, u.bought_cnt};
  order_tree.insert(okey, order_pos);
  order_pos_cache[okey] = order_pos;
  trim_small_cache(order_pos_cache, ORDER_POS_CACHE_LIMIT);
  u.bought_cnt++;
  user_store.update(u, user_pos);
  user_obj_cache[user_id] = u;
  trim_small_cache(user_obj_cache, USER_CACHE_LIMIT);
  out_order = new_order;
  return true;
}
bool UserSystem::add_ticket_loaded(const User &loaded_user,
                                   const Ticket &ticket, int num,
                                   OrderStatus status, order &out_order) {
  if (loaded_user == User())
    return false;
  User u = loaded_user;

  order new_order{ticket, num, u.UserName, status};
  new_order.pos = u.bought_cnt;
  int order_pos = order_store.write(new_order);
  UserOrderKey okey{u.UserName, u.bought_cnt};
  order_tree.insert(okey, order_pos);
  order_pos_cache[okey] = order_pos;
  trim_small_cache(order_pos_cache, ORDER_POS_CACHE_LIMIT);
  u.bought_cnt++;

  int user_pos = 0;
  if (!load_user_pos(user_pos_cache, user_tree, u.UserName, user_pos))
    return false;
  user_store.update(u, user_pos);
  user_obj_cache[u.UserName] = std::move(u);
  trim_small_cache(user_obj_cache, USER_CACHE_LIMIT);
  out_order = new_order;
  return true;
}
bool UserSystem::refund_ticket(String user_id, int pos, order &out_order) {
  int user_pos = 0;
  if (!load_user_pos(user_pos_cache, user_tree, user_id, user_pos))
    return false;
  User u;
  auto cache_it = user_obj_cache.find(user_id);
  if (cache_it != user_obj_cache.end())
    u = cache_it->second;
  else
    user_store.read(u, user_pos);
  if (!u.logged_in)
    return false;
  if (pos < 1 || pos > u.bought_cnt)
    return false;
  int idx = u.bought_cnt - pos;
  order target;
  int order_pos = 0;
  UserOrderKey okey{user_id, idx};
  if (!load_order_pos(order_pos_cache, order_tree, okey, order_pos))
    return false;
  order_store.read(target, order_pos);
  if (target.status == OrderStatus::Refunded) {
    return false;
  }
  order old_target = target;
  target.status = OrderStatus::Refunded;
  order_store.update(target, order_pos);

  out_order = old_target;
  return true;
}
void UserSystem::modify_order(order &o, OrderStatus new_status) {
  order cur;
  UserOrderKey key{o.UserID, o.pos};
  int order_pos = 0;
  if (!load_order_pos(order_pos_cache, order_tree, key, order_pos))
    return;
  order_store.read(cur, order_pos);
  cur.status = new_status;
  order_store.update(cur, order_pos);
}
bool UserSystem::get_order(String user_id, int seq, order &out_order) {
  if (seq < 0)
    return false;
  int order_pos = 0;
  UserOrderKey key{user_id, seq};
  if (!load_order_pos(order_pos_cache, order_tree, key, order_pos))
    return false;
  order_store.read(out_order, order_pos);
  return true;
}
void UserSystem::clean_up() {
  user_tree.clean_up();
  order_tree.clean_up();
  user_store.clean_up();
  order_store.clean_up();
  user_pos_cache.clear();
  user_obj_cache.clear();
  user_brief_cache.clear();
  order_pos_cache.clear();
}
void UserSystem::flush_all() {
  user_tree.flush_all();
  order_tree.flush_all();
}
} // namespace sjtu