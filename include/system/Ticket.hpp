

#pragma once
#include "../memoryriver/memoryriver.hpp"
#include "Train.hpp"
#include "TrainSystem.hpp"
#include <cstdint>
#include <cstring>
#include <string>

using namespace std;
namespace sjtu {

enum class OrderStatus : std::uint8_t {
  Pending = 0,
  Success = 1,
  Refunded = 2,
};

inline const char *status_cstr(OrderStatus status) {
  switch (status) {
  case OrderStatus::Pending:
    return "pending";
  case OrderStatus::Success:
    return "success";
  case OrderStatus::Refunded:
    return "refunded";
  default:
    return "pending";
  }
}

inline OrderStatus status_parse(const std::string &status) {
  if (status == "success")
    return OrderStatus::Success;
  if (status == "refunded")
    return OrderStatus::Refunded;
  return OrderStatus::Pending;
}

class Ticket {
private:
  String trainID;
  String from_station, to_station;
  int time = -1, price = -1;
  static TrainSystem *ptr;
  int date;
  friend class User;
  friend class UserSystem;
  friend class TicketSystem;
  friend class System;

public:
  Ticket() = default;
  Ticket(const Train &train, String trainID, String from_station,
         String to_station, int date)
      : trainID(trainID), from_station(from_station), to_station(to_station),
        date(date) {}
  Ticket(const Ticket &other)
      : trainID(other.trainID), from_station(other.from_station),
        to_station(other.to_station), date(other.date), time(other.time),
        price(other.price) {}
  ~Ticket() = default;
  Ticket &operator=(const Ticket &other) {
    if (this == &other)
      return *this;
    trainID = other.trainID, from_station = other.from_station,
    to_station = other.to_station, date = other.date, time = other.time,
    price = other.price;
    return *this;
  }
  bool operator==(const Ticket &other) const {
    return trainID == other.trainID && from_station == other.from_station &&
           to_station == other.to_station && date == other.date;
  }
  bool operator!=(const Ticket &other) const { return !(*this == other); }

  bool operator<(const Ticket &other) const {
    if (trainID != other.trainID)
      return trainID < other.trainID;
    if (from_station != other.from_station)
      return from_station < other.from_station;
    if (to_station != other.to_station)
      return to_station < other.to_station;
    return date < other.date;
  }
  bool operator<=(const Ticket &other) const { return !(other < *this); }
  bool operator>(const Ticket &other) const { return other < *this; }
  bool operator>=(const Ticket &other) const { return !(*this < other); }

  int getPrice() {
    if (price != -1)
      return price;
    TrainMeta meta;
    if (!ptr->find_train_meta(trainID, meta)) {
      price = -1;
      return -1;
    }
    int from_index = -1, to_index = -1;
    for (int i = 0; i < meta.stationNum; i++) {
      if (meta.stations[i] == from_station)
        from_index = i;
      if (meta.stations[i] == to_station)
        to_index = i;
    }
    int total_price = 0;
    for (int i = from_index; i < to_index; i++) {
      total_price += meta.prices[i];
    }
    price = total_price;
    return total_price;
  }
  int getTime() {
    if (time != -1)
      return time;
    TrainMeta meta;
    if (!ptr->find_train_meta(trainID, meta)) {
      time = -1;
      return -1;
    }
    int from_index = -1, to_index = -1;
    for (int i = 0; i < meta.stationNum; i++) {
      if (meta.stations[i] == from_station)
        from_index = i;
      if (meta.stations[i] == to_station)
        to_index = i;
    }

    if (from_index == -1 || to_index == -1 || from_index >= to_index) {
      time = -1;
      return -1;
    }

    int cur_time = meta.startTime;
    int depart_from = -1;
    int arrive_to = -1;

    for (int i = 0; i < meta.stationNum; ++i) {
      int arrive_i = -1;
      if (i > 0) {
        if (i - 1 >= static_cast<int>(meta.travelTimes.size())) {
          time = -1;
          return -1;
        }
        arrive_i = cur_time + meta.travelTimes[i - 1];
        cur_time = arrive_i;
      }

      if (i == to_index) {
        arrive_to = arrive_i;
      }

      if (i < meta.stationNum - 1) {
        int leave_i;
        if (i == 0) {
          leave_i = cur_time;
        } else {
          if (i - 1 >= static_cast<int>(meta.stopoverTimes.size())) {
            time = -1;
            return -1;
          }
          leave_i = cur_time + meta.stopoverTimes[i - 1];
          cur_time = leave_i;
        }
        if (i == from_index) {
          depart_from = leave_i;
        }
      }
    }

    if (depart_from == -1 || arrive_to == -1 || arrive_to < depart_from) {
      time = -1;
      return -1;
    }

    time = arrive_to - depart_from;
    return time;
  }

  void printTicket(String from_station, String to_station, int num = -1) {
    Train tr;
    if (!ptr->find_train(trainID, tr))
      return;

    cout << tr.ID << ' ' << from_station << ' '
         << tr.getTime(from_station, date) << " -> " << to_station << ' '
         << tr.getTime(to_station, date, 0) << ' ' << getPrice() << ' '
         << (num == -1 ? tr.get_seat_res(from_station, to_station, date) : num)
         << '\n';
  }

  String getID() const { return trainID; }

  void serialize(std::ostream &os) const {
    Serializer<String>::write(os, trainID);
    Serializer<String>::write(os, from_station);
    Serializer<String>::write(os, to_station);
    os.write(reinterpret_cast<const char *>(&date), sizeof(date));
    os.write(reinterpret_cast<const char *>(&time), sizeof(time));
    os.write(reinterpret_cast<const char *>(&price), sizeof(price));
  }
  void deserialize(std::istream &is) {
    Serializer<String>::read(is, trainID);
    Serializer<String>::read(is, from_station);
    Serializer<String>::read(is, to_station);
    is.read(reinterpret_cast<char *>(&date), sizeof(date));
    is.read(reinterpret_cast<char *>(&time), sizeof(time));
    is.read(reinterpret_cast<char *>(&price), sizeof(price));

    if (!is.good()) {
      cerr << "Error: Stream error reading ticket data" << endl;
      is.clear();
      time = -1;
      price = -1;
    }
  }
};

class order {
public:
  Ticket ticket;
  int num;
  OrderStatus status = OrderStatus::Pending;
  String UserID;
  int pos = -1;
  order() : ticket(), num(0), status(OrderStatus::Pending) {}
  order(const Ticket &ticket, int num, String UserID, OrderStatus status)
      : ticket(ticket), num(num), status(status), UserID(UserID) {}
  order(const Ticket &ticket, int num, String UserID, const std::string &status)
      : ticket(ticket), num(num), status(status_parse(status)), UserID(UserID) {
  }
  ~order() = default;
  bool operator==(const order &other) const { return ticket == other.ticket; }
  bool operator!=(const order &other) const { return !(*this == other); }
  bool operator<(const order &other) const {
    if (UserID != other.UserID)
      return UserID < other.UserID;
    if (pos != other.pos)
      return pos < other.pos;
    if (ticket != other.ticket)
      return ticket < other.ticket;
    if (num != other.num)
      return num < other.num;
    return static_cast<std::uint8_t>(status) <
           static_cast<std::uint8_t>(other.status);
  }
  bool operator<=(const order &other) const { return !(other < *this); }
  bool operator>(const order &other) const { return other < *this; }
  bool operator>=(const order &other) const { return !(*this < other); }

  void serialize(std::ostream &os) const {
    Serializer<Ticket>::write(os, ticket);
    os.write(reinterpret_cast<const char *>(&num), sizeof(num));
    Serializer<String>::write(os, UserID);
    std::uint8_t status_code = static_cast<std::uint8_t>(status);
    os.write(reinterpret_cast<const char *>(&status_code), sizeof(status_code));
    os.write(reinterpret_cast<const char *>(&pos), sizeof(pos));
  }
  void deserialize(std::istream &is) {
    Serializer<Ticket>::read(is, ticket);
    is.read(reinterpret_cast<char *>(&num), sizeof(num));
    Serializer<String>::read(is, UserID);
    std::uint8_t status_code = 0;
    is.read(reinterpret_cast<char *>(&status_code), sizeof(status_code));
    if (status_code <= static_cast<std::uint8_t>(OrderStatus::Refunded)) {
      status = static_cast<OrderStatus>(status_code);
    } else {
      status = OrderStatus::Pending;
    }
    is.read(reinterpret_cast<char *>(&pos), sizeof(pos));

    if (!is.good()) {
      cerr << "Error: Stream error reading order data" << endl;
      is.clear();
      num = 0;
      status = OrderStatus::Pending;
      pos = -1;
    }
  }
};

} // namespace sjtu