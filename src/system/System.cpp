#include "../../include/system/System.hpp"
#include "../../include/container/map.hpp"

#include <cstring>
#include <iostream>
#include <string>

using namespace std;
namespace sjtu {
TrainSystem *Ticket::ptr = nullptr;

namespace {
inline int int_min(int a, int b) { return (a < b) ? a : b; }

bool split_write_ok() { return true; }

bool split_buy_ok() { return true; }

class SeatRangeTree {
private:
  static constexpr int MAX_SEG = Train::MAX_STATIONS - 1;
  int n_ = 0;
  int minv_[MAX_SEG * 4 + 5] = {0};
  int lazy_[MAX_SEG * 4 + 5] = {0};

  void apply(int p, int delta) {
    minv_[p] += delta;
    lazy_[p] += delta;
  }

  void push(int p) {
    if (lazy_[p] == 0)
      return;
    apply(p << 1, lazy_[p]);
    apply(p << 1 | 1, lazy_[p]);
    lazy_[p] = 0;
  }

  void pull(int p) { minv_[p] = int_min(minv_[p << 1], minv_[p << 1 | 1]); }

  void build(int p, int l, int r, const int *arr) {
    lazy_[p] = 0;
    if (l == r) {
      minv_[p] = arr[l];
      return;
    }
    int m = (l + r) >> 1;
    build(p << 1, l, m, arr);
    build(p << 1 | 1, m + 1, r, arr);
    pull(p);
  }

  void range_add(int p, int l, int r, int ql, int qr, int delta) {
    if (ql <= l && r <= qr) {
      apply(p, delta);
      return;
    }
    push(p);
    int m = (l + r) >> 1;
    if (ql <= m)
      range_add(p << 1, l, m, ql, qr, delta);
    if (qr > m)
      range_add(p << 1 | 1, m + 1, r, ql, qr, delta);
    pull(p);
  }

  int range_min(int p, int l, int r, int ql, int qr) {
    if (ql <= l && r <= qr)
      return minv_[p];
    push(p);
    int m = (l + r) >> 1;
    int ans = 0x3f3f3f3f;
    if (ql <= m)
      ans = int_min(ans, range_min(p << 1, l, m, ql, qr));
    if (qr > m)
      ans = int_min(ans, range_min(p << 1 | 1, m + 1, r, ql, qr));
    return ans;
  }

  int point_get(int p, int l, int r, int idx) {
    if (l == r)
      return minv_[p];
    push(p);
    int m = (l + r) >> 1;
    if (idx <= m)
      return point_get(p << 1, l, m, idx);
    return point_get(p << 1 | 1, m + 1, r, idx);
  }

public:
  void init(const SeatDay &day) {
    n_ = day.station_num - 1;
    if (n_ <= 0)
      return;
    build(1, 0, n_ - 1, day.seg_seat);
  }

  int query_min(int from, int to_exclusive) {
    if (n_ <= 0 || from < 0 || to_exclusive <= from || to_exclusive > n_)
      return -1;
    return range_min(1, 0, n_ - 1, from, to_exclusive - 1);
  }

  void apply_delta(int from, int to_exclusive, int delta) {
    if (n_ <= 0 || from < 0 || to_exclusive <= from || to_exclusive > n_)
      return;
    range_add(1, 0, n_ - 1, from, to_exclusive - 1, -delta);
  }

  void write_back(SeatDay &day) {
    if (n_ <= 0)
      return;
    for (int i = 0; i < n_; ++i) {
      day.seg_seat[i] = point_get(1, 0, n_ - 1, i);
    }
  }
};

} // namespace

System::System(const std::string &name)
    : ticket_system(name + "_ticket_tree.data"),
      train_system(name + "_train_tree.data"),
      user_system(name + "_user_tree.data"),
      system_river(name + "_system.data") {
  system_river.initialise();
  system_river.get_info(user_cnt, 3);
  Ticket::ptr = &train_system;
  ticket_system.train_system_ptr = &train_system;
}
void System::run() {
  while (!cin.eof()) {
    try {
      timestamp = input.GetTimestamp();
      string command = input.GetCommand();
      cout << "[" << timestamp << "] ";
      if (command == "add_user") {
        add_user();
      } else if (command == "login") {
        login();
      } else if (command == "logout") {
        logout();
      } else if (command == "query_profile") {
        query_profile();
      } else if (command == "modify_profile") {
        modify_profile();
      } else if (command == "add_train") {
        add_train();
      } else if (command == "delete_train") {
        delete_train();
      } else if (command == "release_train") {
        release_train();
      } else if (command == "query_train") {
        query_train();
      } else if (command == "query_ticket") {
        query_ticket();
      } else if (command == "query_transfer") {
        query_transfer_ticket();
      } else if (command == "buy_ticket") {
        buy_ticket();
      } else if (command == "refund_ticket") {
        refund_ticket();
      } else if (command == "query_order") {
        query_order();
      } else if (command == "clean") {
        clean();
      } else if (command == "exit") {
        for (auto &user_id : logged_in_users) {
          user_system.logout(user_id);
        }
        logged_in_users.clear();
        std::cout << "bye\n";

        ticket_system.flush_all();
        train_system.flush_all();
        user_system.flush_all();
        break;
      } else if (command == "test") {

        for (auto &user_id : logged_in_users) {
          user_system.logout(user_id);
        }
        logged_in_users.clear();
      }
    } catch (int) {
      cout << "-1\n";
    }
  }
}
void System::add_user() {
  auto key = input.GetKey();
  String cur_name;
  bool is_first = (user_cnt == 0);
  User new_user;
  while (key != '\n') {
    String str;
    if (key != 'g')
      str = input.GetString();
    if (key == 'u') {
      new_user.UserName = str;
    } else if (key == 'i') {
      new_user.UserName = str;
    } else if (key == 'p') {
      new_user.PassWord = str;
    } else if (key == 'n') {
      new_user.name = str;
    } else if (key == 'm') {
      new_user.MailAdr = str;
    } else if (key == 'g') {
      int pri = input.GetInteger();
      new_user.privilege = pri;
    } else if (key == 'c') {
      cur_name = str;
    }
    key = input.GetKey();
  }

  if (is_first) {
    new_user.privilege = 10;
    user_system.add_user(new_user);
  } else {
    User cur_user;
    if (!user_system.find_user(cur_name, cur_user))
      throw -1;
    if (!cur_user.logged_in)
      throw -1;
    if (cur_user.privilege <= new_user.privilege)
      throw -1;
    User tmp_user;
    if (user_system.find_user(new_user.UserName, tmp_user))
      throw -1;
    user_system.add_user(new_user);
  }
  ++user_cnt;
  system_river.write_info(user_cnt, 3);
  cout << 0 << '\n';
}

void System::login() {
  char key = input.GetKey();
  String user_id;
  String password;
  while (key != '\n') {
    if (key == 'u') {
      user_id = input.GetString();
    } else if (key == 'p') {
      password = input.GetString();
    }
    key = input.GetKey();
  }

  if (!user_system.login(user_id, password))
    throw -1;
  logged_in_users.push_back(user_id);
  cout << 0 << '\n';
}

void System::logout() {
  char key = input.GetKey();
  String user_id = input.GetString();
  if (!user_system.logout(user_id))
    throw -1;
  cout << 0 << '\n';
}

void System::query_profile() {
  char key = input.GetKey();
  String cur_id, user_id;
  while (key != '\n') {
    if (key == 'u') {
      user_id = input.GetString();
    } else if (key == 'c') {
      cur_id = input.GetString();
    }
    key = input.GetKey();
  }
  int cur_privilege = 0;
  bool cur_logged_in = false;
  if (!user_system.get_user_brief(cur_id, cur_privilege, cur_logged_in))
    throw -1;
  if (!cur_logged_in)
    throw -1;

  User u;
  if (!user_system.find_user(user_id, u))
    throw -1;
  if (cur_privilege <= u.privilege && cur_id != user_id)
    throw -1;
  cout << u.UserName << ' ' << u.name << ' ' << u.MailAdr << ' ' << u.privilege
       << '\n';
}

void System::modify_profile() {
  char key = input.GetKey();
  String cur_username;
  String target_username;
  bool qualified = true;
  User tmp;
  bool isg = 0, isp = 0, isn = 0, ism = 0;
  while (key != '\n') {
    String str;
    if (key != 'g')
      str = input.GetString();
    if (key == 'p') {
      tmp.PassWord = str;
      isp = 1;
    } else if (key == 'n') {
      tmp.name = str;
      isn = 1;
    } else if (key == 'm') {
      tmp.MailAdr = str;
      ism = 1;
    } else if (key == 'g') {
      int pri = input.GetInteger();
      tmp.privilege = pri;
      isg = 1;
    } else if (key == 'u') {
      target_username = str;
    } else if (key == 'c') {
      cur_username = str;
    }
    key = input.GetKey();
  }
  User original_user;
  if (!user_system.find_user(target_username, original_user)) {
    throw -1;
  }

  User cur_user;
  if (!user_system.find_user(cur_username, cur_user)) {
    throw -1;
  }

  if (!cur_user.logged_in) {
    throw -1;
  }

  User target_user = original_user;
  if (isp)
    target_user.PassWord = tmp.PassWord;
  if (isn)
    target_user.name = tmp.name;
  if (ism)
    target_user.MailAdr = tmp.MailAdr;
  if (isg)
    target_user.privilege = tmp.privilege;
  if (original_user != cur_user &&
      original_user.privilege >= cur_user.privilege) {
    throw -1;
  }

  if (isg && target_user.privilege >= cur_user.privilege) {
    throw -1;
  }

  if (!user_system.modify_user(original_user.UserName, target_user))
    throw -1;
  cout << target_user.UserName << ' ' << target_user.name << ' '
       << target_user.MailAdr << ' ' << target_user.privilege << '\n';
}

void System::add_train() {
  auto key = input.GetKey();
  Train new_train;
  while (key != '\n') {
    String str;
    int num;
    if (key == 'i') {
      new_train.ID = input.GetString();
    } else if (key == 'n') {
      new_train.stationNum = input.GetInteger();
    } else if (key == 'm') {
      new_train.seatNum = input.GetInteger();
    } else if (key == 's') {
      new_train.stations = input.GetStringArray();
    } else if (key == 'p') {
      new_train.prices = input.GetIntegerArray();
    } else if (key == 'x') {
      new_train.startTime = input.GetTime();
    } else if (key == 't') {
      new_train.travelTimes = input.GetIntegerArray();
    } else if (key == 'o') {
      new_train.stopoverTimes = input.GetIntegerArray();
    } else if (key == 'd') {
      new_train.sale_begin = input.GetDate();

      new_train.sale_end = input.GetDate();
    } else if (key == 'y') {
      new_train.type = input.GetChar();
    }
    key = input.GetKey();
  }
  if (!train_system.add_train(new_train))
    throw -1;
  cout << 0 << '\n';
}

void System::delete_train() {
  char key = input.GetKey();
  String train_id = input.GetString();
  if (!train_system.delete_train(train_id))
    throw -1;
  cout << 0 << '\n';
}

void System::release_train() {
  char key = input.GetKey();
  String train_id = input.GetString();
  Train released_train;
  if (!train_system.release_train(train_id, &released_train))
    throw -1;
  ticket_system.add_ticket(released_train);
  cout << 0 << '\n';
}

void System::query_train() {
  char key = input.GetKey();
  String train_id;
  int date;
  while (key != '\n') {
    if (key == 'i') {
      train_id = input.GetString();
    } else if (key == 'd') {
      date = input.GetDate();
    }
    key = input.GetKey();
  }
  Train train;
  if (!train_system.find_train(train_id, train))
    throw -1;
  if (date < train.sale_begin || date > train.sale_end)
    throw -1;
  int time = train.startTime;
  int price = 0;

  cout << train.ID << ' ' << train.type << '\n';
  for (int i = 0; i < train.stationNum; i++) {
    String arr_time, leave_time;
    if (i == 0)
      arr_time = "xx-xx xx:xx";
    else {
      arr_time = train.realTime(time + train.travelTimes[i - 1], date);
      time += train.travelTimes[i - 1];
    }

    if (i == train.stationNum - 1)
      leave_time = "xx-xx xx:xx";
    else if (i == 0)
      leave_time = train.realTime(time, date);
    else {
      leave_time = train.realTime(time + train.stopoverTimes[i - 1], date);
      time += train.stopoverTimes[i - 1];
    }

    int res_seat;
    if (i == train.stationNum - 1)
      res_seat = 0;
    else if (!train.is_released())
      res_seat = train.seatNum;
    else {
      if (!train_system.get_seat_idx(train.ID, i, i + 1, date, res_seat)) {
        res_seat = -1;
      }
    }

    cout << train.stations[i] << ' ' << arr_time << " -> " << leave_time << ' '
         << price << ' ';
    if (i != train.stationNum - 1) {
      price += train.prices[i];
      cout << res_seat << '\n';
    } else
      cout << 'x' << '\n';
  }
}
void System::query_ticket() {
  char key = input.GetKey();
  String from_station, to_station;
  int date;
  CompareType cmp_type = PRICE;
  while (key != '\n') {
    if (key == 's') {
      from_station = input.GetString();
    } else if (key == 't') {
      to_station = input.GetString();
    } else if (key == 'd') {
      date = input.GetDate();
    } else if (key == 'p') {
      String get_type = input.GetString();
      if (get_type == (string) "time")
        cmp_type = TIME;
      else
        cmp_type = PRICE;
    }
    key = input.GetKey();
  }

  if (!ticket_system.query_ticket(from_station, to_station, date, cmp_type))
    throw -1;
}

void System::query_transfer_ticket() {
  char key = input.GetKey();
  String from_station, to_station;
  int date;
  CompareType cmp_type = PRICE;
  while (key != '\n') {
    if (key == 's') {
      from_station = input.GetString();
    } else if (key == 't') {
      to_station = input.GetString();
    } else if (key == 'd') {
      date = input.GetDate();
    } else if (key == 'p') {
      String get_type = input.GetString();
      if (get_type == (string) "time")
        cmp_type = TIME;
      else
        cmp_type = PRICE;
    }
    key = input.GetKey();
  }
  if (!ticket_system.query_transfer_ticket(from_station, to_station, date,
                                           cmp_type))
    throw -1;
}

void System::buy_ticket() {
  char key = input.GetKey();
  Ticket ticket;
  int num;
  bool if_wait = false;
  String user_id;
  while (key != '\n') {
    if (key == 'u') {
      user_id = input.GetString();
    } else if (key == 'i') {
      ticket.trainID = input.GetString();
    } else if (key == 'd') {
      ticket.date = input.GetDate();
    } else if (key == 'f') {
      ticket.from_station = input.GetString();
    } else if (key == 't') {
      ticket.to_station = input.GetString();
    } else if (key == 'n') {
      num = input.GetInteger();
    } else if (key == 'q') {
      String c = input.GetString();
      if (c == (string)("true"))
        if_wait = true;
    }
    key = input.GetKey();
  }
  User find_user;
  if (!user_system.find_user(user_id, find_user))
    throw -1;
  if (!find_user.logged_in)
    throw -1;
  TrainMeta tr_meta;
  if (!train_system.find_train_meta(ticket.trainID, tr_meta) ||
      !tr_meta.released)
    throw -1;

  order result(ticket, num, user_id, "");
  int pos = -1, to_pos = -1;
  for (int i = 0; i < tr_meta.stationNum; i++) {
    if (tr_meta.stations[i] == ticket.from_station)
      pos = i;
    if (tr_meta.stations[i] == ticket.to_station)
      to_pos = i;
  }
  if (pos == -1 || to_pos == -1 || pos >= to_pos)
    throw -1;
  ticket.date -= tr_meta.date[pos];

  bool split_buy = split_buy_ok();
  bool split_write = split_write_ok();
  bool need_train_snapshot = !(split_buy && split_write);
  Train tr;
  Train *tr_ptr = nullptr;
  if (need_train_snapshot || !split_write) {
    if (!train_system.find_train(ticket.trainID, tr) || !tr.is_released())
      throw -1;
    tr_ptr = &tr;
  }

  if (!ticket_system.buy_ticket(tr_meta, tr_ptr, ticket, num, if_wait, result,
                                user_id, pos, to_pos))
    throw -1;

  order temp_order;
  if (!user_system.add_ticket_loaded(find_user, ticket, num, result.status,
                                     temp_order))
    throw -1;
  if (result.status == OrderStatus::Success) {
    if (!split_write) {
      tr.set_seat_idx(pos, to_pos, ticket.date, num);
      train_system.update_train_snapshot(ticket.trainID, tr);
    }
    if (!train_system.set_seat_idx(ticket.trainID, pos, to_pos, ticket.date,
                                   num)) {
    }
  } else if (result.status == OrderStatus::Pending) {
    int waiting_user_pos = 0;
    if (!user_system.get_user_pos(user_id, waiting_user_pos))
      throw -1;
    int waiting_id =
        ticket_system.append_waiting_order(temp_order, waiting_user_pos);
    int waiting_train_serial = 0;
    if (!ticket_system.get_train_serial(temp_order.ticket.trainID,
                                        waiting_train_serial))
      throw -1;
    TicketSystem::WaitingBucketKey key{waiting_train_serial,
                                       temp_order.ticket.date};
    ticket_system.waiting_pos_tree.insert(key, waiting_id);
    ticket_system.mark_waiting_dirty();
  }
}

void System::refund_ticket() {
  char key = input.GetKey();
  String user_id;
  Ticket ticket;
  int num = 1;
  while (key != '\n') {
    if (key == 'u') {
      user_id = input.GetString();
    } else if (key == 'n') {
      num = input.GetInteger();
    }
    key = input.GetKey();
  }

  order refunded_order;
  if (!user_system.refund_ticket(user_id, num, refunded_order))
    throw -1;

  if (refunded_order.status == OrderStatus::Pending) {
    bool waiting_changed = false;
    int refunded_train_serial = 0;
    if (!ticket_system.get_train_serial(refunded_order.ticket.trainID,
                                        refunded_train_serial))
      throw -1;
    int refunded_user_pos = 0;
    if (!user_system.get_user_pos(refunded_order.UserID, refunded_user_pos))
      throw -1;
    TicketSystem::WaitingBucketKey bucket_key{refunded_train_serial,
                                              refunded_order.ticket.date};
    auto bucket = ticket_system.waiting_pos_tree.find(bucket_key);
    for (int i = 0; i < bucket.size(); ++i) {
      int waiting_id = bucket[i].value;
      TicketSystem::WaitingOrderRef wait_ref;
      if (!ticket_system.get_waiting_order(waiting_id, wait_ref)) {
        ticket_system.waiting_pos_tree.erase(bucket_key, waiting_id);
        waiting_changed = true;
        continue;
      }
      if (wait_ref.status != OrderStatus::Pending)
        continue;
      if (wait_ref.user_pos == refunded_user_pos &&
          wait_ref.order_seq == refunded_order.pos) {
        wait_ref.status = OrderStatus::Refunded;
        ticket_system.update_waiting_order(waiting_id, wait_ref);
        ticket_system.waiting_pos_tree.erase(bucket_key, waiting_id);
        waiting_changed = true;
        break;
      }
    }
    if (waiting_changed)
      ticket_system.mark_waiting_dirty();
    cout << 0 << '\n';
    return;
  }

  Train refund_train;
  if (!train_system.find_train(refunded_order.ticket.trainID, refund_train) ||
      !refund_train.is_released())
    throw -1;
  map<String, int> station_index;
  for (int i = 0; i < refund_train.stationNum; ++i) {
    if (station_index.find(refund_train.stations[i]) == station_index.end()) {
      station_index[refund_train.stations[i]] = i;
    }
  }
  int refund_from = -1, refund_to = -1;
  auto refund_from_it = station_index.find(refunded_order.ticket.from_station);
  if (refund_from_it != station_index.end())
    refund_from = refund_from_it->second;
  auto refund_to_it = station_index.find(refunded_order.ticket.to_station);
  if (refund_to_it != station_index.end())
    refund_to = refund_to_it->second;
  if (refund_from == -1 || refund_to == -1 || refund_from >= refund_to)
    throw -1;
  TrainMeta refund_meta;
  SeatDay base_day;
  SeatDay work_day;
  bool day_batch_on = false;
  SeatRangeTree seat_range_tree;
  if (split_write_ok()) {
    if (!train_system.find_train_meta(refunded_order.ticket.trainID,
                                      refund_meta) ||
        !refund_meta.released)
      throw -1;
    if (!train_system.ensure_seat_day(refunded_order.ticket.trainID,
                                      refunded_order.ticket.date, base_day)) {
      throw -1;
    }
    if (base_day.station_num <= 0)
      base_day.station_num = refund_meta.stationNum;
    work_day = base_day;
    seat_range_tree.init(work_day);
    day_batch_on = true;
  }

  auto day_min_query = [&](int from, int to) -> int {
    return seat_range_tree.query_min(from, to);
  };

  auto day_add_delta = [&](int from, int to, int delta) {
    seat_range_tree.apply_delta(from, to, delta);
  };

  if (!split_write_ok()) {
    refund_train.update_seat_res(
        refunded_order.ticket.from_station, refunded_order.ticket.to_station,
        refunded_order.ticket.date, -refunded_order.num);
  } else {
    day_add_delta(refund_from, refund_to, -refunded_order.num);
  }
  if (!split_write_ok()) {
    if (!train_system.set_seat_idx(refunded_order.ticket.trainID, refund_from,
                                   refund_to, refunded_order.ticket.date,
                                   -refunded_order.num)) {
    }
  }

  bool waiting_changed = false;
  int refunded_train_serial = 0;
  if (!ticket_system.get_train_serial(refunded_order.ticket.trainID,
                                      refunded_train_serial))
    throw -1;
  TicketSystem::WaitingBucketKey bucket_key{refunded_train_serial,
                                            refunded_order.ticket.date};
  auto bucket = ticket_system.waiting_pos_tree.find(bucket_key);
  for (int i = 0; i < bucket.size(); ++i) {
    int waiting_id = bucket[i].value;
    TicketSystem::WaitingOrderRef wait_ref;
    if (!ticket_system.get_waiting_order(waiting_id, wait_ref)) {
      ticket_system.waiting_pos_tree.erase(bucket_key, waiting_id);
      waiting_changed = true;
      continue;
    }
    if (wait_ref.status != OrderStatus::Pending)
      continue;

    User wait_user;
    order wait_user_order;
    if (!user_system.get_user_at(wait_ref.user_pos, wait_user) ||
        wait_ref.order_seq < 0 || wait_ref.order_seq >= wait_user.bought_cnt ||
        !user_system.get_order(wait_user.UserName, wait_ref.order_seq,
                               wait_user_order) ||
        wait_user_order.status != OrderStatus::Pending) {
      wait_ref.status = OrderStatus::Refunded;
      ticket_system.update_waiting_order(waiting_id, wait_ref);
      ticket_system.waiting_pos_tree.erase(bucket_key, waiting_id);
      waiting_changed = true;
      continue;
    }

    int wait_from = -1, wait_to = -1;
    auto wait_from_it = station_index.find(wait_user_order.ticket.from_station);
    if (wait_from_it != station_index.end())
      wait_from = wait_from_it->second;
    auto wait_to_it = station_index.find(wait_user_order.ticket.to_station);
    if (wait_to_it != station_index.end())
      wait_to = wait_to_it->second;
    if (wait_from == -1 || wait_to == -1 || refund_from == -1 ||
        refund_to == -1 || !(wait_from < refund_to && refund_from < wait_to)) {
      continue;
    }

    int seat_res = -1;
    if (split_write_ok()) {
      seat_res = day_min_query(wait_from, wait_to);
      if (seat_res < 0) {
        continue;
      }
    } else {
      seat_res = refund_train.get_seat_res(wait_user_order.ticket.from_station,
                                           wait_user_order.ticket.to_station,
                                           wait_user_order.ticket.date);
    }
    if (seat_res < wait_user_order.num)
      continue;

    if (!split_write_ok()) {
      refund_train.update_seat_res(wait_user_order.ticket.from_station,
                                   wait_user_order.ticket.to_station,
                                   wait_user_order.ticket.date,
                                   wait_user_order.num);
    } else {
      day_add_delta(wait_from, wait_to, wait_user_order.num);
    }

    if (!split_write_ok()) {
      if (!train_system.set_seat_idx(wait_user_order.ticket.trainID, wait_from,
                                     wait_to, wait_user_order.ticket.date,
                                     wait_user_order.num)) {
      }
    }
    user_system.modify_order(wait_user_order, OrderStatus::Success);
    wait_ref.status = OrderStatus::Success;
    ticket_system.update_waiting_order(waiting_id, wait_ref);
    ticket_system.waiting_pos_tree.erase(bucket_key, waiting_id);
    waiting_changed = true;
  }

  if (day_batch_on) {
    seat_range_tree.write_back(work_day);
    if (!train_system.write_seat_day(refunded_order.ticket.trainID,
                                     refunded_order.ticket.date, base_day,
                                     work_day)) {
    }
  }

  if (!split_write_ok()) {
    train_system.update_train_snapshot(refunded_order.ticket.trainID,
                                       refund_train);
  }

  if (waiting_changed)
    ticket_system.mark_waiting_dirty();
  cout << 0 << '\n';
}

void System::query_order() {
  char key = input.GetKey();

  String user_id = input.GetString();

  if (!user_system.query_ordered_tickets(user_id))
    throw -1;
}

void System::clean() {
  user_cnt = 0;
  system_river.write_info(user_cnt, 3);
  train_system.clean_up();
  user_system.clean_up();
  ticket_system.clean_up();
  cout << 0 << '\n';
}

} // namespace sjtu