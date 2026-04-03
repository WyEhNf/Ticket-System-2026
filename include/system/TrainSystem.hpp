#pragma once
#include "../container/bpt.hpp"
#include "../container/hashmap.hpp"
#include "../container/list.hpp"
#include "../container/vector.hpp"
#include "Train.hpp"
#include <cstdint>
using namespace std;
namespace sjtu {
struct TrainMeta {
  char type = 0;
  String ID;
  bool released = false;
  int stationNum = 0, seatNum = 0;
  int startTime = 0, sale_begin = 0, sale_end = 0;
  vector<String> stations;
  vector<int> prices, travelTimes, stopoverTimes;
  vector<int> date;

  static constexpr int MAX_VECTOR_SIZE = Train::MAX_VECTOR_SIZE;

  void from_train(const Train &t) {
    type = t.type;
    ID = t.ID;
    released = t.released;
    stationNum = t.stationNum;
    seatNum = t.seatNum;
    startTime = t.startTime;
    sale_begin = t.sale_begin;
    sale_end = t.sale_end;
    stations = t.stations;
    prices = t.prices;
    travelTimes = t.travelTimes;
    stopoverTimes = t.stopoverTimes;
    date = t.date;
  }

  bool operator==(const TrainMeta &other) const { return ID == other.ID; }
  bool operator!=(const TrainMeta &other) const { return !(*this == other); }
  bool operator<(const TrainMeta &other) const { return ID < other.ID; }
  bool operator<=(const TrainMeta &other) const { return !(other < *this); }
  bool operator>(const TrainMeta &other) const { return other < *this; }
  bool operator>=(const TrainMeta &other) const { return !(*this < other); }

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

    size_t sz = 0;
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
  }
};

struct SeatKey {
  String train_id;
  int date = 0;

  bool operator==(const SeatKey &o) const {
    return train_id == o.train_id && date == o.date;
  }
  bool operator!=(const SeatKey &o) const { return !(*this == o); }
  bool operator<(const SeatKey &o) const {
    if (train_id != o.train_id)
      return train_id < o.train_id;
    return date < o.date;
  }
  bool operator<=(const SeatKey &o) const { return !(o < *this); }
  bool operator>(const SeatKey &o) const { return o < *this; }
  bool operator>=(const SeatKey &o) const { return !(*this < o); }

  std::size_t hash() const {
    std::size_t h1 = train_id.hash();
    std::size_t h2 = hash_mix64(static_cast<std::size_t>(date));
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
  }

  void serialize(std::ostream &os) const {
    Serializer<String>::write(os, train_id);
    os.write(reinterpret_cast<const char *>(&date), sizeof(date));
  }
  void deserialize(std::istream &is) {
    Serializer<String>::read(is, train_id);
    is.read(reinterpret_cast<char *>(&date), sizeof(date));
  }
};

struct SeatDay {
  static constexpr std::uint32_t SERIAL_MAGIC_V2_U16 = 0x53443231u;
  static constexpr std::uint32_t SERIAL_MAGIC_V3_U24 = 0x53443331u;
  int station_num = 0;
  int seg_seat[Train::MAX_STATIONS - 1] = {0};

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

  int active_seg_count() const {
    int cnt = station_num - 1;
    if (cnt < 0)
      cnt = 0;
    if (cnt > Train::MAX_STATIONS - 1)
      cnt = Train::MAX_STATIONS - 1;
    return cnt;
  }

  bool operator==(const SeatDay &o) const {
    if (station_num != o.station_num)
      return false;
    int seg_cnt = active_seg_count();
    for (int i = 0; i < seg_cnt; ++i) {
      if (seg_seat[i] != o.seg_seat[i])
        return false;
    }
    return true;
  }
  bool operator!=(const SeatDay &o) const { return !(*this == o); }
  bool operator<(const SeatDay &o) const {
    if (station_num != o.station_num)
      return station_num < o.station_num;
    int seg_cnt = active_seg_count();
    for (int i = 0; i < seg_cnt; ++i) {
      if (seg_seat[i] != o.seg_seat[i])
        return seg_seat[i] < o.seg_seat[i];
    }
    return false;
  }
  bool operator<=(const SeatDay &o) const { return !(o < *this); }
  bool operator>(const SeatDay &o) const { return o < *this; }
  bool operator>=(const SeatDay &o) const { return !(*this < o); }

  void serialize(std::ostream &os) const {
    os.write(reinterpret_cast<const char *>(&station_num), sizeof(station_num));
    int seg_cnt = active_seg_count();
    bool can_use_u24 = true;
    for (int i = 0; i < seg_cnt; ++i) {
      if (seg_seat[i] < 0 || seg_seat[i] > 0xFFFFFF) {
        can_use_u24 = false;
        break;
      }
    }

    if (can_use_u24) {
      std::uint32_t magic = SERIAL_MAGIC_V3_U24;
      os.write(reinterpret_cast<const char *>(&magic), sizeof(magic));
      for (int i = 0; i < seg_cnt; ++i) {
        write_u24(os, seg_seat[i]);
      }
      return;
    }

    if (seg_cnt > 0) {
      os.write(reinterpret_cast<const char *>(seg_seat),
               sizeof(seg_seat[0]) * static_cast<size_t>(seg_cnt));
    }
  }
  void deserialize(std::istream &is) {
    is.read(reinterpret_cast<char *>(&station_num), sizeof(station_num));
    for (int i = 0; i < Train::MAX_STATIONS - 1; ++i)
      seg_seat[i] = 0;
    int seg_cnt = active_seg_count();
    std::streampos payload_pos = is.tellg();
    std::uint32_t magic = 0;
    is.read(reinterpret_cast<char *>(&magic), sizeof(magic));
    if (!is.good())
      return;

    if (magic == SERIAL_MAGIC_V3_U24) {
      for (int i = 0; i < seg_cnt; ++i) {
        seg_seat[i] = read_u24(is);
      }
      return;
    }

    if (magic == SERIAL_MAGIC_V2_U16) {
      for (int i = 0; i < seg_cnt; ++i) {
        std::uint16_t compact = 0;
        is.read(reinterpret_cast<char *>(&compact), sizeof(compact));
        seg_seat[i] = static_cast<int>(compact);
      }
      return;
    }

    is.clear();
    is.seekg(payload_pos);
    if (seg_cnt > 0) {
      is.read(reinterpret_cast<char *>(seg_seat),
              sizeof(seg_seat[0]) * static_cast<size_t>(seg_cnt));
    }
  }
};

} // namespace sjtu

namespace sjtu {

class TrainSystem {
private:
  BPlusTree<String, int> train_tree;
  BPlusTree<String, int> train_meta_tree;
  BPlusTree<SeatKey, int> seat_tree;
  MemoryRiver<Train> train_store;
  MemoryRiver<TrainMeta> train_meta_store;
  MemoryRiver<SeatDay> seat_store;

  struct SeatDayCacheEntry {
    SeatDay day;
    int seat_pos = -1;
    bool dirty = false;
    list<SeatKey>::iterator lru_it;
  };

  static constexpr std::size_t SEAT_DAY_CACHE_LIMIT = 256;
  list<SeatKey> seat_day_lru_;
  hashmap<SeatKey, SeatDayCacheEntry> seat_day_cache_;

  void touch_day_cache(hashmap<SeatKey, SeatDayCacheEntry>::iterator it);
  void evict_day_cache();
  void flush_day_cache();
  bool load_day_data(const SeatKey &key, SeatDay &out_day, int &out_pos);
  void cache_day_data(const SeatKey &key, const SeatDay &day, int pos,
                      bool dirty);

  bool ensure_seat_day(const String &train_id, int date, SeatDay &out_day);
  bool write_seat_day(const String &train_id, int date, const SeatDay &old_day,
                      const SeatDay &new_day);
  friend class Ticket;
  friend class TicketSystem;
  friend class System;

public:
  TrainSystem(string filename = "train_tree.data")
      : train_tree(filename, 6), train_meta_tree(filename + ".meta", 6),
        seat_tree(filename + ".seat_idx", 6),
        train_store(filename + ".blob_train_v2"),
        train_meta_store(filename + ".blob_meta_v2"),
        seat_store(filename + ".blob_seat") {
    train_store.initialise(filename + ".blob_train_v2");
    train_meta_store.initialise(filename + ".blob_meta_v2");
    seat_store.initialise(filename + ".blob_seat");
  }
  ~TrainSystem() {}
  bool add_train(const Train &new_train);
  bool delete_train(const String &train_id);
  bool find_train(const String &train_id, Train &out_train);
  bool find_train_meta(const String &train_id, TrainMeta &out_meta);
  bool get_seat_day(const String &train_id, int date, SeatDay &out_day);
  bool get_seat_idx(const String &train_id, int from, int to, int date,
                    int &result);
  bool set_seat_idx(const String &train_id, int from, int to, int date,
                    int delta);
  bool update_train_snapshot(const String &train_id, const Train &old_train,
                             const Train &new_train);
  bool update_train_snapshot(const String &train_id, const Train &new_train);
  bool release_train(const String &train_id, Train *released_train = nullptr);
  void clean_up();
  void flush_all();
};
} // namespace sjtu