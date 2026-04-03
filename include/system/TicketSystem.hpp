#include "../container/bpt.hpp"
#include "../container/hashmap.hpp"
#include "../container/list.hpp"
#include "../container/vector.hpp"
#include "Ticket.hpp"
#include "TrainSystem.hpp"
#include <cstdint>
#include <string>
using namespace std;
namespace sjtu {
enum CompareType { TIME, PRICE };
class TicketSystem {
private:
  struct TicketKey {
    int date;
    int station_serial = 0;
    bool operator==(const TicketKey &o) const {
      return date == o.date && station_serial == o.station_serial;
    }
    bool operator!=(const TicketKey &o) const { return !(*this == o); }
    bool operator<(const TicketKey &o) const {
      if (date != o.date)
        return date < o.date;
      return station_serial < o.station_serial;
    }
    bool operator>(const TicketKey &o) const { return o < *this; }
    bool operator<=(const TicketKey &o) const { return !(o < *this); }
    bool operator>=(const TicketKey &o) const { return !(*this < o); }

    void serialize(std::ostream &os) const {
      os.write(reinterpret_cast<const char *>(&date), sizeof(date));
      os.write(reinterpret_cast<const char *>(&station_serial),
               sizeof(station_serial));
    }
    void deserialize(std::istream &is) {
      is.read(reinterpret_cast<char *>(&date), sizeof(date));
      is.read(reinterpret_cast<char *>(&station_serial),
              sizeof(station_serial));
    }
  };
  struct TicketRef {
    int train_serial = 0;
    int depart_day = 0;
    int from_idx = 0;

    bool operator==(const TicketRef &o) const {
      return train_serial == o.train_serial && depart_day == o.depart_day &&
             from_idx == o.from_idx;
    }
    bool operator!=(const TicketRef &o) const { return !(*this == o); }
    bool operator<(const TicketRef &o) const {
      if (train_serial != o.train_serial)
        return train_serial < o.train_serial;
      if (depart_day != o.depart_day)
        return depart_day < o.depart_day;
      return from_idx < o.from_idx;
    }
    bool operator>(const TicketRef &o) const { return o < *this; }
    bool operator<=(const TicketRef &o) const { return !(o < *this); }
    bool operator>=(const TicketRef &o) const { return !(*this < o); }

    void serialize(std::ostream &os) const {
      os.write(reinterpret_cast<const char *>(&train_serial),
               sizeof(train_serial));
      os.write(reinterpret_cast<const char *>(&depart_day), sizeof(depart_day));
      os.write(reinterpret_cast<const char *>(&from_idx), sizeof(from_idx));
    }
    void deserialize(std::istream &is) {
      is.read(reinterpret_cast<char *>(&train_serial), sizeof(train_serial));
      is.read(reinterpret_cast<char *>(&depart_day), sizeof(depart_day));
      is.read(reinterpret_cast<char *>(&from_idx), sizeof(from_idx));
    }
  };
  struct DirectPairKey {
    int from_station_serial = 0;
    int to_station_serial = 0;

    bool operator==(const DirectPairKey &o) const {
      return from_station_serial == o.from_station_serial &&
             to_station_serial == o.to_station_serial;
    }
    bool operator!=(const DirectPairKey &o) const { return !(*this == o); }
    bool operator<(const DirectPairKey &o) const {
      if (from_station_serial != o.from_station_serial)
        return from_station_serial < o.from_station_serial;
      return to_station_serial < o.to_station_serial;
    }
    bool operator>(const DirectPairKey &o) const { return o < *this; }
    bool operator<=(const DirectPairKey &o) const { return !(o < *this); }
    bool operator>=(const DirectPairKey &o) const { return !(*this < o); }

    void serialize(std::ostream &os) const {
      os.write(reinterpret_cast<const char *>(&from_station_serial),
               sizeof(from_station_serial));
      os.write(reinterpret_cast<const char *>(&to_station_serial),
               sizeof(to_station_serial));
    }
    void deserialize(std::istream &is) {
      is.read(reinterpret_cast<char *>(&from_station_serial),
              sizeof(from_station_serial));
      is.read(reinterpret_cast<char *>(&to_station_serial),
              sizeof(to_station_serial));
    }
  };
  struct DirectPairRef {
    int train_serial = 0;
    int from_idx = 0;
    int to_idx = 0;
  };
  struct WaitingBucketKey {
    int train_serial = 0;
    int date = 0;

    bool operator==(const WaitingBucketKey &o) const {
      return train_serial == o.train_serial && date == o.date;
    }
    bool operator!=(const WaitingBucketKey &o) const { return !(*this == o); }
    bool operator<(const WaitingBucketKey &o) const {
      if (train_serial != o.train_serial)
        return train_serial < o.train_serial;
      return date < o.date;
    }
    bool operator>(const WaitingBucketKey &o) const { return o < *this; }
    bool operator<=(const WaitingBucketKey &o) const { return !(o < *this); }
    bool operator>=(const WaitingBucketKey &o) const { return !(*this < o); }
    void serialize(std::ostream &os) const {
      os.write(reinterpret_cast<const char *>(&train_serial),
               sizeof(train_serial));
      os.write(reinterpret_cast<const char *>(&date), sizeof(date));
    }
    void deserialize(std::istream &is) {
      is.read(reinterpret_cast<char *>(&train_serial), sizeof(train_serial));
      is.read(reinterpret_cast<char *>(&date), sizeof(date));
    }
  };
  BPlusTree<TicketKey, std::uint64_t, 128> ticket_tree;
  BPlusTree<DirectPairKey, std::uint64_t, 128> direct_ticket_tree;
  BPlusTree<String, int, 8> train_serial_tree;
  BPlusTree<int, String, 8> serial_train_tree;
  BPlusTree<String, int, 8> station_serial_tree;
  BPlusTree<int, String, 8> serial_station_tree;
  BPlusTree<int, int, 64> station_train_tree;
  BPlusTree<int, std::uint64_t, 64> station_pos_tree;
  vector<order> waiting_list;

  struct WaitingOrderRef {
    int user_pos = -1;
    int order_seq = -1;
    OrderStatus status = OrderStatus::Pending;

    void serialize(std::ostream &os) const {
      os.write(reinterpret_cast<const char *>(&user_pos), sizeof(user_pos));
      os.write(reinterpret_cast<const char *>(&order_seq), sizeof(order_seq));
      std::uint8_t status_code = static_cast<std::uint8_t>(status);
      os.write(reinterpret_cast<const char *>(&status_code),
               sizeof(status_code));
    }

    void deserialize(std::istream &is) {
      is.read(reinterpret_cast<char *>(&user_pos), sizeof(user_pos));
      is.read(reinterpret_cast<char *>(&order_seq), sizeof(order_seq));
      std::uint8_t status_code = 0;
      is.read(reinterpret_cast<char *>(&status_code), sizeof(status_code));
      if (status_code <= static_cast<std::uint8_t>(OrderStatus::Refunded)) {
        status = static_cast<OrderStatus>(status_code);
      } else {
        status = OrderStatus::Pending;
      }
    }
  };

  BPlusTree<WaitingBucketKey, int> waiting_pos_tree;
  BPlusTree<int, int, 8> wait_pos_tree;
  MemoryRiver<WaitingOrderRef> waiting_order_store;
  TrainSystem *train_system_ptr;
  std::string waiting_file;
  std::string waiting_meta_file;
  std::string serial_meta_file;
  std::string station_meta_file;
  int next_waiting_id = 1;
  int next_train_serial = 1;
  int next_station_serial = 1;
  bool train_meta_dirty = false;
  bool station_meta_dirty = false;

  static std::uint64_t pack_ticket_ref(const TicketRef &ref) {
    return (static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(ref.train_serial))
            << 32) |
           (static_cast<std::uint64_t>(
                static_cast<std::uint16_t>(ref.depart_day))
            << 16) |
           static_cast<std::uint16_t>(ref.from_idx);
  }
  static TicketRef unpack_ticket_ref(std::uint64_t packed) {
    TicketRef ref;
    ref.train_serial =
        static_cast<int>(static_cast<std::uint32_t>(packed >> 32));
    ref.depart_day =
        static_cast<int>(static_cast<std::uint16_t>((packed >> 16) & 0xFFFFu));
    ref.from_idx =
        static_cast<int>(static_cast<std::uint16_t>(packed & 0xFFFFu));
    return ref;
  }
  static std::uint64_t pack_pair_ref(const DirectPairRef &ref) {
    return (static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(ref.train_serial))
            << 32) |
           (static_cast<std::uint64_t>(static_cast<std::uint16_t>(ref.from_idx))
            << 16) |
           static_cast<std::uint16_t>(ref.to_idx);
  }
  static DirectPairRef unpack_pair_ref(std::uint64_t packed) {
    DirectPairRef ref;
    ref.train_serial =
        static_cast<int>(static_cast<std::uint32_t>(packed >> 32));
    ref.from_idx =
        static_cast<int>(static_cast<std::uint16_t>((packed >> 16) & 0xFFFFu));
    ref.to_idx = static_cast<int>(static_cast<std::uint16_t>(packed & 0xFFFFu));
    return ref;
  }
  static std::uint64_t pack_st_pos(int train_serial, int station_pos) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(train_serial))
            << 32) |
           static_cast<std::uint32_t>(station_pos);
  }
  static void unpack_st_pos(std::uint64_t packed, int &train_serial,
                            int &station_pos) {
    train_serial = static_cast<int>(static_cast<std::uint32_t>(packed >> 32));
    station_pos =
        static_cast<int>(static_cast<std::uint32_t>(packed & 0xFFFFFFFFu));
  }

  bool waiting_dirty = false;
  int waiting_dirty_ops = 0;
  static constexpr int WAITING_SAVE_THRESHOLD = 128;

  struct QueryTicketCacheKey {
    int from_station_serial = 0;
    int to_station_serial = 0;
    int date = 0;
    int cmp_type = 0;

    bool operator==(const QueryTicketCacheKey &o) const {
      return from_station_serial == o.from_station_serial &&
             to_station_serial == o.to_station_serial && date == o.date &&
             cmp_type == o.cmp_type;
    }
  };

  struct QueryTicketCacheKeyHash {
    std::size_t operator()(const QueryTicketCacheKey &key) const {
      std::size_t h = static_cast<std::size_t>(key.from_station_serial);
      h = h * 1315423911u + static_cast<std::size_t>(key.to_station_serial);
      h = h * 1315423911u + static_cast<std::size_t>(key.date);
      h = h * 1315423911u + static_cast<std::size_t>(key.cmp_type);
      return h;
    }
  };

  struct CachedQueryTicketItem {
    int train_serial = 0;
    int depart_day = 0;
    int from_idx = -1;
    int to_idx = -1;
    int price = 0;
    int time = 0;
  };

  struct TrainQueryLite {
    bool valid = false;
    String train_id;
    int station_num = 0;
    int sale_begin = 0;
    int sale_end = 0;
    vector<int> station_serials;
    vector<int> date;
    vector<int> prefix_price;
    vector<int> depart_time;
    vector<int> arrive_time;
  };

  using StationPositionsByTrain = hashmap<int, vector<int>>;
  using CachedQueryTicketVec = vector<CachedQueryTicketItem>;

  static constexpr std::size_t STATION_POS_CACHE_LIMIT = 12;
  static constexpr std::size_t QUERY_TICKET_CACHE_LIMIT = 48;

  list<int> station_pos_lru_;
  hashmap<int, pair<StationPositionsByTrain, list<int>::iterator>>
      station_pos_cache_;

  list<QueryTicketCacheKey> query_ticket_lru_;
  hashmap<QueryTicketCacheKey,
          pair<CachedQueryTicketVec, list<QueryTicketCacheKey>::iterator>,
          QueryTicketCacheKeyHash>
      query_ticket_cache_;
  vector<TrainQueryLite> train_lite_cache_;

  void invalidate_query_caches();
  void upsert_train_lite(int train_serial, const TrainMeta &train_meta);
  TrainQueryLite *get_train_lite(int train_serial);
  const StationPositionsByTrain *get_pos_cache(int station_serial);
  bool get_qt_cache(const QueryTicketCacheKey &key,
                    CachedQueryTicketVec &out_vec);
  void put_qt_cache(const QueryTicketCacheKey &key,
                    const CachedQueryTicketVec &vec);

  void load_waiting_list();
  void save_waiting_list();
  void mark_waiting_dirty();
  void rebuild_waiting_index();
  void compact_waiting_list();
  void load_waiting_meta();
  void save_waiting_meta();
  int append_waiting_order(const order &o, int user_pos);
  bool get_waiting_order(int waiting_id, WaitingOrderRef &out_order);
  bool update_waiting_order(int waiting_id, const WaitingOrderRef &new_order);
  void load_serial_meta();
  void save_serial_meta();
  bool get_train_serial(const String &train_id, int &train_serial);
  int get_train_sid(const String &train_id);
  bool resolve_train_id(int train_serial, String &train_id);
  void load_station_meta();
  void save_station_meta();
  bool get_station_serial(const String &station, int &station_serial);
  int get_station_sid(const String &station);
  bool resolve_station_name(int station_serial, String &station);
  int get_price_sum(const vector<int> &prices, int from_idx, int to_idx);
  friend class System;

public:
  TicketSystem(string filename)
      : ticket_tree(filename + ".idx_v3", 8),
        direct_ticket_tree(filename + ".direct_pair_idx_v1", 8),
        train_serial_tree(filename + ".serial_map_v2", 6),
        serial_train_tree(filename + ".serial_rev_v2", 6),
        station_serial_tree(filename + ".station_map_v1", 6),
        serial_station_tree(filename + ".station_rev_v1", 6),
        station_train_tree(filename + ".station_train_v1", 6),
        station_pos_tree(filename + ".station_train_pos_v1", 6),
        waiting_pos_tree(filename + ".waiting_idx", 2),
        wait_pos_tree(filename + ".waiting_pos_v2", 2),
        waiting_order_store(filename + ".waiting_ref_v4"),
        waiting_file(filename + ".waiting"),
        waiting_meta_file(filename + ".waiting_meta_v2"),
        serial_meta_file(filename + ".serial_meta_v2"),
        station_meta_file(filename + ".station_meta_v1") {
    waiting_order_store.initialise(filename + ".waiting_ref_v4");
    load_waiting_meta();
    load_serial_meta();
    load_station_meta();
    load_waiting_list();
  }
  ~TicketSystem() {}
  bool add_ticket(const Train &train);
  bool query_ticket(const String &from_station, const String &to_station,
                    int date, CompareType cmp_type);
  bool query_transfer_ticket(const String &from_station,
                             const String &to_station, int date,
                             CompareType cmp_type);
  bool buy_ticket(const TrainMeta &tr_meta, Train *tr_snapshot, Ticket &ticket,
                  int num, bool if_wait, order &result, const String &UserID,
                  int from_idx = -1, int to_idx = -1);
  bool refund_ticket(const Ticket &ticket, int num, order &out_order);

  static bool Compare_with_cost(Ticket &A, Ticket &B);
  static bool Compare_with_time(Ticket &A, Ticket &B);
  void printTicket(const Ticket &t, String from_station, String to_station);
  void clean_up();
  void flush_all();
};
} // namespace sjtu