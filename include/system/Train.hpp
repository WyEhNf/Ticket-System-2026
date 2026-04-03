

#pragma once
#include "../container/String.hpp"
#include "../container/vector.hpp"
#include "../memoryriver/memoryriver.hpp"
#include <cstdint>
#include <utility>
using namespace std;
namespace sjtu {
class Train {
public:
  static constexpr int MAX_STATIONS = 100;
  static constexpr int MAX_DAYS = 92;
  static constexpr int MAX_VECTOR_SIZE = 100;

  char type;
  String ID;
  bool released = false;
  int stationNum, seatNum;
  int startTime, sale_begin, sale_end;
  vector<String> stations;
  int *seat_res;
  vector<int> prices, travelTimes, stopoverTimes;
  vector<int> date;
  friend class Ticket;
  friend class TrainSystem;
  friend class UserSystem;
  friend class TicketSystem;
  friend class System;

private:
  static constexpr int SEAT_RES_SIZE = MAX_DAYS * (MAX_STATIONS - 1);
  static constexpr std::uint32_t SEAT_SERIAL_MAGIC_U24 = 0x54525333u;
  static constexpr std::uint32_t SEAT_SERIAL_MAGIC_I32 = 0x54525334u;

  static void write_u24(std::ostream &os, int value) {
    std::uint32_t v = static_cast<std::uint32_t>(value);
    unsigned char b0 = static_cast<unsigned char>(v & 0xFFu);
    unsigned char b1 = static_cast<unsigned char>((v >> 8) & 0xFFu);
    unsigned char b2 = static_cast<unsigned char>((v >> 16) & 0xFFu);
    os.write(reinterpret_cast<const char *>(&b0), 1);
    os.write(reinterpret_cast<const char *>(&b1), 1);
    os.write(reinterpret_cast<const char *>(&b2), 1);
  }

  static int read_u24(std::istream &is) {
    unsigned char b0 = 0;
    unsigned char b1 = 0;
    unsigned char b2 = 0;
    is.read(reinterpret_cast<char *>(&b0), 1);
    is.read(reinterpret_cast<char *>(&b1), 1);
    is.read(reinterpret_cast<char *>(&b2), 1);
    std::uint32_t v = static_cast<std::uint32_t>(b0) |
                      (static_cast<std::uint32_t>(b1) << 8) |
                      (static_cast<std::uint32_t>(b2) << 16);
    return static_cast<int>(v);
  }

  int active_day_count() const {
    int d = sale_end - sale_begin + 1;
    if (d < 0)
      d = 0;
    if (d > MAX_DAYS)
      d = MAX_DAYS;
    return d;
  }

  int seg_count() const {
    int s = stationNum - 1;
    if (s < 0)
      s = 0;
    if (s > MAX_STATIONS - 1)
      s = MAX_STATIONS - 1;
    return s;
  }

  int seat_value_count() const { return active_day_count() * seg_count(); }

  void ensure_seat_res() {
    if (seat_res != nullptr)
      return;
    int seat_cnt = seat_value_count();
    if (seat_cnt <= 0)
      return;
    seat_res = new int[seat_cnt];
    for (int i = 0; i < seat_cnt; ++i)
      seat_res[i] = 0;
  }

public:
  Train()
      : released(false), stationNum(0), seatNum(0), startTime(0), sale_begin(0),
        sale_end(0), seat_res(nullptr) {}
  Train(char type, String ID, int stationNum, int seatNum, int startTime,
        int sale_begin, int sale_end, vector<String> stations,
        vector<int> prices, vector<int> travelTimes, vector<int> stopoverTimes)
      : type(type), ID(ID), stationNum(stationNum), seatNum(seatNum),
        startTime(startTime), sale_begin(sale_begin), sale_end(sale_end),
        stations(stations), prices(prices), travelTimes(travelTimes),
        stopoverTimes(stopoverTimes), released(false), seat_res(nullptr) {
    ensure_seat_res();
  }
  Train(const Train &other)
      : type(other.type), ID(other.ID), stationNum(other.stationNum),
        seatNum(other.seatNum), startTime(other.startTime),
        sale_begin(other.sale_begin), sale_end(other.sale_end),
        stations(other.stations), prices(other.prices),
        travelTimes(other.travelTimes), stopoverTimes(other.stopoverTimes),
        released(other.released), date(other.date), seat_res(nullptr) {
    if (other.seat_res != nullptr) {
      int seat_cnt = seat_value_count();
      if (seat_cnt > 0) {
        seat_res = new int[seat_cnt];
        for (int i = 0; i < seat_cnt; ++i)
          seat_res[i] = other.seat_res[i];
      }
    }
  }

  Train(Train &&other) noexcept
      : type(other.type), ID(std::move(other.ID)), released(other.released),
        stationNum(other.stationNum), seatNum(other.seatNum),
        startTime(other.startTime), sale_begin(other.sale_begin),
        sale_end(other.sale_end), stations(std::move(other.stations)),
        prices(std::move(other.prices)),
        travelTimes(std::move(other.travelTimes)),
        stopoverTimes(std::move(other.stopoverTimes)),
        date(std::move(other.date)), seat_res(other.seat_res) {
    other.seat_res = nullptr;
    other.released = false;
    other.stationNum = 0;
    other.seatNum = 0;
  }
  ~Train() {
    delete[] seat_res;
    seat_res = nullptr;
  }
  Train &operator=(const Train &other) {
    if (this == &other)
      return *this;
    type = other.type, ID = other.ID, stationNum = other.stationNum,
    seatNum = other.seatNum;
    startTime = other.startTime, sale_begin = other.sale_begin,
    sale_end = other.sale_end;
    stations = other.stations, prices = other.prices,
    travelTimes = other.travelTimes;
    stopoverTimes = other.stopoverTimes;
    released = other.released;
    date = other.date;
    delete[] seat_res;
    seat_res = nullptr;
    if (other.seat_res != nullptr) {
      int seat_cnt = seat_value_count();
      if (seat_cnt > 0) {
        seat_res = new int[seat_cnt];
        for (int i = 0; i < seat_cnt; ++i)
          seat_res[i] = other.seat_res[i];
      }
    }
    return *this;
  }

  Train &operator=(Train &&other) noexcept {
    if (this == &other)
      return *this;
    type = other.type;
    ID = std::move(other.ID);
    stationNum = other.stationNum;
    seatNum = other.seatNum;
    startTime = other.startTime;
    sale_begin = other.sale_begin;
    sale_end = other.sale_end;
    stations = std::move(other.stations);
    prices = std::move(other.prices);
    travelTimes = std::move(other.travelTimes);
    stopoverTimes = std::move(other.stopoverTimes);
    released = other.released;
    date = std::move(other.date);
    delete[] seat_res;
    seat_res = other.seat_res;
    other.seat_res = nullptr;
    other.released = false;
    other.stationNum = 0;
    other.seatNum = 0;
    return *this;
  }
  void initialise() {
    date.clear();
    int time = startTime;
    int base = startTime;
    date.push_back(0);
    for (int i = 1; i < stationNum; i++) {
      time += travelTimes[i - 1];
      if (i != stationNum - 1)
        time += stopoverTimes[i - 1];
      date.push_back((time / 1440 - base / 1440));
    }
  }
  bool operator==(const Train &other) const { return ID == other.ID; }
  bool operator!=(const Train &other) const { return ID != other.ID; }
  bool operator<(const Train &other) const { return ID < other.ID; }
  bool operator<=(const Train &other) const { return ID <= other.ID; }
  bool operator>(const Train &other) const { return !(*this <= other); }
  bool operator>=(const Train &other) const { return !(*this < other); }

  void serialize(std::ostream &os) const {
    os.write(&type, sizeof(type));
    Serializer<String>::write(os, ID);
    os.write(reinterpret_cast<const char *>(&released), sizeof(released));
    os.write(reinterpret_cast<const char *>(&stationNum), sizeof(stationNum));
    os.write(reinterpret_cast<const char *>(&seatNum), sizeof(seatNum));
    os.write(reinterpret_cast<const char *>(&startTime), sizeof(startTime));
    os.write(reinterpret_cast<const char *>(&sale_begin), sizeof(sale_begin));
    os.write(reinterpret_cast<const char *>(&sale_end), sizeof(sale_end));

    size_t sz = stations.size();
    os.write(reinterpret_cast<const char *>(&sz), sizeof(sz));
    for (size_t i = 0; i < sz; ++i)
      Serializer<String>::write(os, stations[i]);
    for (size_t i = sz; i < MAX_VECTOR_SIZE; ++i) {
      String empty;
      Serializer<String>::write(os, empty);
    }

    sz = prices.size();
    os.write(reinterpret_cast<const char *>(&sz), sizeof(sz));
    for (size_t i = 0; i < sz; ++i)
      os.write(reinterpret_cast<const char *>(&prices[i]), sizeof(int));
    for (size_t i = sz; i < MAX_VECTOR_SIZE; ++i) {
      int zero = 0;
      os.write(reinterpret_cast<const char *>(&zero), sizeof(int));
    }

    sz = travelTimes.size();
    os.write(reinterpret_cast<const char *>(&sz), sizeof(sz));
    for (size_t i = 0; i < sz; ++i)
      os.write(reinterpret_cast<const char *>(&travelTimes[i]), sizeof(int));
    for (size_t i = sz; i < MAX_VECTOR_SIZE; ++i) {
      int zero = 0;
      os.write(reinterpret_cast<const char *>(&zero), sizeof(int));
    }

    sz = stopoverTimes.size();
    os.write(reinterpret_cast<const char *>(&sz), sizeof(sz));
    for (size_t i = 0; i < sz; ++i)
      os.write(reinterpret_cast<const char *>(&stopoverTimes[i]), sizeof(int));
    for (size_t i = sz; i < MAX_VECTOR_SIZE; ++i) {
      int zero = 0;
      os.write(reinterpret_cast<const char *>(&zero), sizeof(int));
    }

    sz = date.size();
    if (sz > MAX_VECTOR_SIZE)
      sz = MAX_VECTOR_SIZE;
    os.write(reinterpret_cast<const char *>(&sz), sizeof(sz));
    for (size_t i = 0; i < sz; ++i)
      os.write(reinterpret_cast<const char *>(&date[i]), sizeof(int));
    for (size_t i = sz; i < MAX_VECTOR_SIZE; ++i) {
      int zero = 0;
      os.write(reinterpret_cast<const char *>(&zero), sizeof(int));
    }

    int seat_full[SEAT_RES_SIZE] = {0};
    if (seat_res != nullptr) {
      int stride = seg_count();
      int days = active_day_count();
      for (int d = 0; d < days; ++d) {
        for (int i = 0; i < stride; ++i) {
          seat_full[d * (MAX_STATIONS - 1) + i] = seat_res[d * stride + i];
        }
      }
    }
    os.write(reinterpret_cast<const char *>(seat_full),
             static_cast<std::streamsize>(sizeof(seat_full)));
  }

  void deserialize(std::istream &is) {
    is.read(&type, sizeof(type));
    Serializer<String>::read(is, ID);
    is.read(reinterpret_cast<char *>(&released), sizeof(released));
    is.read(reinterpret_cast<char *>(&stationNum), sizeof(stationNum));
    is.read(reinterpret_cast<char *>(&seatNum), sizeof(seatNum));
    is.read(reinterpret_cast<char *>(&startTime), sizeof(startTime));
    is.read(reinterpret_cast<char *>(&sale_begin), sizeof(sale_begin));
    is.read(reinterpret_cast<char *>(&sale_end), sizeof(sale_end));

    size_t sz;
    is.read(reinterpret_cast<char *>(&sz), sizeof(sz));
    stations.clear();
    for (size_t i = 0; i < sz; ++i) {
      String s;
      Serializer<String>::read(is, s);
      stations.push_back(s);
    }
    for (size_t i = sz; i < MAX_VECTOR_SIZE; ++i) {
      String s;
      Serializer<String>::read(is, s);
    }

    is.read(reinterpret_cast<char *>(&sz), sizeof(sz));
    prices.clear();
    for (size_t i = 0; i < sz; ++i) {
      int val;
      is.read(reinterpret_cast<char *>(&val), sizeof(int));
      prices.push_back(val);
    }
    for (size_t i = sz; i < MAX_VECTOR_SIZE; ++i) {
      int val;
      is.read(reinterpret_cast<char *>(&val), sizeof(int));
    }

    is.read(reinterpret_cast<char *>(&sz), sizeof(sz));
    travelTimes.clear();
    for (size_t i = 0; i < sz; ++i) {
      int val;
      is.read(reinterpret_cast<char *>(&val), sizeof(int));
      travelTimes.push_back(val);
    }
    for (size_t i = sz; i < MAX_VECTOR_SIZE; ++i) {
      int val;
      is.read(reinterpret_cast<char *>(&val), sizeof(int));
    }

    is.read(reinterpret_cast<char *>(&sz), sizeof(sz));
    stopoverTimes.clear();
    for (size_t i = 0; i < sz; ++i) {
      int val;
      is.read(reinterpret_cast<char *>(&val), sizeof(int));
      stopoverTimes.push_back(val);
    }
    for (size_t i = sz; i < MAX_VECTOR_SIZE; ++i) {
      int val;
      is.read(reinterpret_cast<char *>(&val), sizeof(int));
    }

    is.read(reinterpret_cast<char *>(&sz), sizeof(sz));
    date.clear();
    for (size_t i = 0; i < sz; ++i) {
      int val;
      is.read(reinterpret_cast<char *>(&val), sizeof(int));
      date.push_back(val);
    }
    for (size_t i = sz; i < MAX_VECTOR_SIZE; ++i) {
      int val;
      is.read(reinterpret_cast<char *>(&val), sizeof(int));
    }

    int seat_full[SEAT_RES_SIZE] = {0};
    is.read(reinterpret_cast<char *>(seat_full),
            static_cast<std::streamsize>(sizeof(seat_full)));

    int expected_cnt = seat_value_count();
    delete[] seat_res;
    seat_res = nullptr;
    if (expected_cnt > 0) {
      seat_res = new int[expected_cnt];
      for (int i = 0; i < expected_cnt; ++i)
        seat_res[i] = 0;
    }

    if (seat_res != nullptr) {
      int stride = seg_count();
      int days = active_day_count();
      for (int d = 0; d < days; ++d) {
        for (int i = 0; i < stride; ++i) {
          seat_res[d * stride + i] = seat_full[d * (MAX_STATIONS - 1) + i];
        }
      }
    }

    if (date.empty()) {
      initialise();
    }
  }
  void release() { released = true; }
  bool is_released() { return released; }
  int get_seat_res(String from_station, String to_station, int date) {
    if (seat_res == nullptr)
      return -1;
    int day_index = date - sale_begin;
    int stride = seg_count();
    int days = active_day_count();
    if (day_index < 0 || day_index >= days || stride <= 0)
      return -1;
    int from = -1, to = -1;
    for (int i = 0; i < stationNum; i++) {
      if (stations[i] == from_station)
        from = i;
      if (stations[i] == to_station)
        to = i;
    }
    if (from == -1 || to == -1 || from >= to)
      return -1;
    int min_seat = seat_res[day_index * stride + from];
    for (int i = from; i < to; i++) {
      if (seat_res[day_index * stride + i] < min_seat)
        min_seat = seat_res[day_index * stride + i];
    }
    return min_seat;
  }
  int get_seat_idx(int from, int to, int date) {
    if (seat_res == nullptr)
      return -1;
    int day_index = date - sale_begin;
    int stride = seg_count();
    int days = active_day_count();
    if (day_index < 0 || day_index >= days || stride <= 0)
      return -1;
    if (from < 0 || to < 0 || from >= to || to > stationNum)
      return -1;

    int min_seat = seat_res[day_index * stride + from];
    for (int i = from; i < to; ++i) {
      int cur = seat_res[day_index * stride + i];
      if (cur < min_seat)
        min_seat = cur;
    }
    return min_seat;
  }
  void update_seat_res(String from_station, String to_station, int date,
                       int num) {
    if (seat_res == nullptr)
      return;
    int day_index = date - sale_begin;
    int stride = seg_count();
    int days = active_day_count();
    if (day_index < 0 || day_index >= days || stride <= 0)
      return;
    int from = -1, to = -1;
    for (int i = 0; i < stationNum; i++) {
      if (stations[i] == from_station)
        from = i;
      if (stations[i] == to_station)
        to = i;
    }
    if (from == -1 || to == -1 || from >= to)
      return;
    for (int i = from; i < to; i++) {
      seat_res[day_index * stride + i] -= num;
    }
  }
  void set_seat_idx(int from, int to, int date, int num) {
    if (seat_res == nullptr)
      return;
    int day_index = date - sale_begin;
    int stride = seg_count();
    int days = active_day_count();
    if (day_index < 0 || day_index >= days || stride <= 0)
      return;
    if (from < 0 || to < 0 || from >= to || to > stationNum)
      return;

    for (int i = from; i < to; ++i) {
      seat_res[day_index * stride + i] -= num;
    }
  }
  string int_to_date(int date) const {

    static const int month_days[12] = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30, 31};
    int month = 6;
    int day = date;
    while (day > month_days[month - 1]) {
      day -= month_days[month - 1];
      month++;
      if (month > 12)
        month = 1;
    }
    return String::FromInt(month) + "-" + String::FromInt(day);
  }
  string int_to_time(int time) const {
    int hour = time / 60;
    int minute = time % 60;
    return String::FromInt(hour) + ":" + String::FromInt(minute);
  }
  String realTime(int time, int date) const {
    int realdate = date + time / 1440;
    int realtime = time % 1440;
    return int_to_date(realdate) + " " + int_to_time(realtime);
  }
  String getTime(String station, int date, bool isArrive = 1) {
    int time = startTime;
    if (stations[0] == station)
      return realTime(time, date);
    for (int i = 1; i < stationNum; i++) {
      time += travelTimes[i - 1];
      if (stations[i] == station && !isArrive)
        return realTime(time, date);
      if (i != stationNum - 1)
        time += stopoverTimes[i - 1];
      if (stations[i] == station && isArrive)
        return realTime(time, date);
    }
    return String("error!");
  }
};

} // namespace sjtu
