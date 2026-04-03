#include "../../include/system/TicketSystem.hpp"
#include "../../include/container/map.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace std;
namespace sjtu {

namespace {
struct TrainQueryCache {
  TrainMeta meta;
  vector<int> prefix_price;
  vector<int> depart_time;
  vector<int> arrive_time;
};

void build_train_cache(const TrainMeta &train, TrainQueryCache &cache) {
  cache.meta = train;
  cache.prefix_price.clear();
  cache.depart_time.clear();
  cache.arrive_time.clear();

  cache.prefix_price.push_back(0);
  for (int i = 0; i < train.stationNum - 1; ++i) {
    cache.prefix_price.push_back(cache.prefix_price[i] + train.prices[i]);
  }

  for (int i = 0; i < train.stationNum; ++i) {
    cache.depart_time.push_back(0);
    cache.arrive_time.push_back(-1);
  }

  int cur_time = train.startTime;
  cache.depart_time[0] = cur_time;
  cache.arrive_time[0] = -1;
  for (int i = 1; i < train.stationNum; ++i) {
    cur_time += train.travelTimes[i - 1];
    cache.arrive_time[i] = cur_time;
    if (i < train.stationNum - 1) {
      cur_time += train.stopoverTimes[i - 1];
      cache.depart_time[i] = cur_time;
    }
  }
}

TrainQueryCache *get_train_cache(const String &train_id,
                                 map<String, TrainQueryCache> &cache_map,
                                 TrainSystem *train_system) {
  auto it = cache_map.find(train_id);
  if (it != cache_map.end())
    return &it->second;

  TrainMeta train;
  if (!train_system->find_train_meta(train_id, train) || train.ID == String() ||
      !train.released)
    return nullptr;

  cache_map[train_id] = TrainQueryCache{};
  auto inserted = cache_map.find(train_id);
  build_train_cache(train, inserted->second);
  return &inserted->second;
}

Train g_time_formatter;
String real_time(int time, int date) {
  return g_time_formatter.realTime(time, date);
}

int first_position_greater(const vector<int> &positions, int from_idx) {
  int l = 0;
  int r = static_cast<int>(positions.size());
  while (l < r) {
    int mid = l + ((r - l) >> 1);
    if (positions[mid] <= from_idx)
      l = mid + 1;
    else
      r = mid;
  }
  if (l >= static_cast<int>(positions.size()))
    return -1;
  return positions[l];
}

template <typename Vec, typename Compare>
void quick_sort_vec(Vec &arr, int left, int right, Compare comp) {
  int i = left;
  int j = right;
  auto pivot = arr[left + ((right - left) >> 1)];
  while (i <= j) {
    while (comp(arr[i], pivot))
      ++i;
    while (comp(pivot, arr[j]))
      --j;
    if (i <= j) {
      if (i != j) {
        auto tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
      }
      ++i;
      --j;
    }
  }
  if (left < j)
    quick_sort_vec(arr, left, j, comp);
  if (i < right)
    quick_sort_vec(arr, i, right, comp);
}

template <typename Vec, typename Compare>
void sort_vec(Vec &arr, Compare comp) {
  if (arr.size() <= 1)
    return;
  quick_sort_vec(arr, 0, static_cast<int>(arr.size()) - 1, comp);
}

bool split_read_on() { return true; }

bool split_buy_on() { return true; }

bool split_write_on() { return true; }

} // namespace

void TicketSystem::invalidate_query_caches() {
  station_pos_cache_.clear();
  station_pos_lru_.clear();
  query_ticket_cache_.clear();
  query_ticket_lru_.clear();
}

void TicketSystem::upsert_train_lite(int train_serial,
                                     const TrainMeta &train_meta) {
  if (train_serial <= 0 || train_meta.stationNum <= 0)
    return;
  if (train_lite_cache_.size() <= static_cast<std::size_t>(train_serial)) {
    train_lite_cache_.resize(static_cast<std::size_t>(train_serial) + 1);
  }

  TrainQueryLite &cache = train_lite_cache_[train_serial];
  cache.valid = true;
  cache.train_id = train_meta.ID;
  cache.station_num = train_meta.stationNum;
  cache.sale_begin = train_meta.sale_begin;
  cache.sale_end = train_meta.sale_end;
  cache.station_serials.clear();
  cache.station_serials.reserve(train_meta.stations.size());
  for (int i = 0; i < static_cast<int>(train_meta.stations.size()); ++i) {
    int station_serial = 0;
    if (!get_station_serial(train_meta.stations[i], station_serial)) {
      station_serial = 0;
    }
    cache.station_serials.push_back(station_serial);
  }
  cache.date = train_meta.date;

  cache.prefix_price.clear();
  cache.depart_time.clear();
  cache.arrive_time.clear();

  cache.prefix_price.push_back(0);
  for (int i = 0; i < train_meta.stationNum - 1; ++i) {
    cache.prefix_price.push_back(cache.prefix_price.back() +
                                 train_meta.prices[i]);
  }

  cache.depart_time.assign(train_meta.stationNum, 0);
  cache.arrive_time.assign(train_meta.stationNum, -1);

  int cur_time = train_meta.startTime;
  cache.depart_time[0] = cur_time;
  for (int i = 1; i < train_meta.stationNum; ++i) {
    cur_time += train_meta.travelTimes[i - 1];
    cache.arrive_time[i] = cur_time;
    if (i < train_meta.stationNum - 1) {
      cur_time += train_meta.stopoverTimes[i - 1];
      cache.depart_time[i] = cur_time;
    }
  }
}

TicketSystem::TrainQueryLite *TicketSystem::get_train_lite(int train_serial) {
  if (train_serial <= 0)
    return nullptr;
  if (train_lite_cache_.size() <= static_cast<std::size_t>(train_serial)) {
    train_lite_cache_.resize(static_cast<std::size_t>(train_serial) + 1);
  }

  TrainQueryLite &cache = train_lite_cache_[train_serial];
  if (cache.valid)
    return &cache;

  String train_id;
  if (!resolve_train_id(train_serial, train_id))
    return nullptr;

  TrainMeta train_meta;
  if (!train_system_ptr->find_train_meta(train_id, train_meta) ||
      train_meta.ID == String() || !train_meta.released) {
    return nullptr;
  }

  upsert_train_lite(train_serial, train_meta);
  return &train_lite_cache_[train_serial];
}

const TicketSystem::StationPositionsByTrain *
TicketSystem::get_pos_cache(int station_serial) {
  auto it = station_pos_cache_.find(station_serial);
  if (it != station_pos_cache_.end()) {
    station_pos_lru_.splice(station_pos_lru_.begin(), station_pos_lru_,
                            it->second.second);
    return &it->second.first;
  }

  StationPositionsByTrain positions_by_train;
  station_pos_tree.for_each_key(station_serial, [&](const auto &item) {
    int train_serial = 0;
    int station_pos = 0;
    unpack_st_pos(item.value, train_serial, station_pos);
    positions_by_train[train_serial].push_back(station_pos);
  });

  if (station_pos_cache_.size() >= STATION_POS_CACHE_LIMIT &&
      !station_pos_lru_.empty()) {
    int evict_station_serial = station_pos_lru_.back();
    station_pos_lru_.pop_back();
    station_pos_cache_.erase(evict_station_serial);
  }

  station_pos_lru_.push_front(station_serial);
  auto inserted = station_pos_cache_.emplace(
      station_serial,
      pair<StationPositionsByTrain, list<int>::iterator>(
          std::move(positions_by_train), station_pos_lru_.begin()));
  return &inserted.first->second.first;
}

bool TicketSystem::get_qt_cache(const QueryTicketCacheKey &key,
                                CachedQueryTicketVec &out_vec) {
  auto it = query_ticket_cache_.find(key);
  if (it == query_ticket_cache_.end())
    return false;
  query_ticket_lru_.splice(query_ticket_lru_.begin(), query_ticket_lru_,
                           it->second.second);
  out_vec = it->second.first;
  return true;
}

void TicketSystem::put_qt_cache(const QueryTicketCacheKey &key,
                                const CachedQueryTicketVec &vec) {
  auto it = query_ticket_cache_.find(key);
  if (it != query_ticket_cache_.end()) {
    it->second.first = vec;
    query_ticket_lru_.splice(query_ticket_lru_.begin(), query_ticket_lru_,
                             it->second.second);
    return;
  }

  if (query_ticket_cache_.size() >= QUERY_TICKET_CACHE_LIMIT &&
      !query_ticket_lru_.empty()) {
    QueryTicketCacheKey evict_key = query_ticket_lru_.back();
    query_ticket_lru_.pop_back();
    query_ticket_cache_.erase(evict_key);
  }

  query_ticket_lru_.push_front(key);
  query_ticket_cache_.emplace(
      key, pair<CachedQueryTicketVec, list<QueryTicketCacheKey>::iterator>(
               vec, query_ticket_lru_.begin()));
}

void TicketSystem::rebuild_waiting_index() {}

void TicketSystem::compact_waiting_list() { waiting_list.clear(); }

void TicketSystem::load_waiting_list() {
  waiting_list.clear();

  waiting_dirty = false;
  waiting_dirty_ops = 0;
}

void TicketSystem::load_waiting_meta() {
  next_waiting_id = 1;
  if (!std::filesystem::exists(waiting_meta_file))
    return;
  std::ifstream is(waiting_meta_file, std::ios::binary);
  if (!is.good())
    return;
  int saved = 1;
  is.read(reinterpret_cast<char *>(&saved), sizeof(saved));
  if (is.good() && saved > 0)
    next_waiting_id = saved;
}

void TicketSystem::save_waiting_meta() {
  std::ofstream os(waiting_meta_file, std::ios::binary | std::ios::trunc);
  if (!os.good())
    return;
  os.write(reinterpret_cast<const char *>(&next_waiting_id),
           sizeof(next_waiting_id));
}

int TicketSystem::append_waiting_order(const order &o, int user_pos) {
  int waiting_id = next_waiting_id++;
  WaitingOrderRef ref;
  ref.user_pos = user_pos;
  ref.order_seq = o.pos;
  ref.status = o.status;
  int pos = waiting_order_store.write(ref);
  wait_pos_tree.insert(waiting_id, pos);
  return waiting_id;
}

bool TicketSystem::get_waiting_order(int waiting_id,
                                     WaitingOrderRef &out_order) {
  int pos = 0;
  if (!wait_pos_tree.find_first_value(waiting_id, pos))
    return false;
  waiting_order_store.read(out_order, pos);
  return true;
}

bool TicketSystem::update_waiting_order(int waiting_id,
                                        const WaitingOrderRef &new_order) {
  int pos = 0;
  if (!wait_pos_tree.find_first_value(waiting_id, pos))
    return false;
  waiting_order_store.update(new_order, pos);
  return true;
}

void TicketSystem::load_serial_meta() {
  next_train_serial = 1;
  train_meta_dirty = false;
  if (!std::filesystem::exists(serial_meta_file))
    return;
  std::ifstream is(serial_meta_file, std::ios::binary);
  if (!is.good())
    return;
  int saved = 1;
  is.read(reinterpret_cast<char *>(&saved), sizeof(saved));
  if (is.good() && saved > 0)
    next_train_serial = saved;
}

void TicketSystem::save_serial_meta() {
  std::ofstream os(serial_meta_file, std::ios::binary | std::ios::trunc);
  if (!os.good())
    return;
  os.write(reinterpret_cast<const char *>(&next_train_serial),
           sizeof(next_train_serial));
  train_meta_dirty = false;
}

bool TicketSystem::get_train_serial(const String &train_id, int &train_serial) {
  return train_serial_tree.find_first_value(train_id, train_serial);
}

int TicketSystem::get_train_sid(const String &train_id) {
  int serial = 0;
  if (get_train_serial(train_id, serial))
    return serial;
  serial = next_train_serial++;
  train_serial_tree.insert(train_id, serial);
  serial_train_tree.insert(serial, train_id);
  train_meta_dirty = true;
  return serial;
}

bool TicketSystem::resolve_train_id(int train_serial, String &train_id) {
  return serial_train_tree.find_first_value(train_serial, train_id);
}

void TicketSystem::load_station_meta() {
  next_station_serial = 1;
  station_meta_dirty = false;
  if (!std::filesystem::exists(station_meta_file))
    return;
  std::ifstream is(station_meta_file, std::ios::binary);
  if (!is.good())
    return;
  int saved = 1;
  is.read(reinterpret_cast<char *>(&saved), sizeof(saved));
  if (is.good() && saved > 0)
    next_station_serial = saved;
}

void TicketSystem::save_station_meta() {
  std::ofstream os(station_meta_file, std::ios::binary | std::ios::trunc);
  if (!os.good())
    return;
  os.write(reinterpret_cast<const char *>(&next_station_serial),
           sizeof(next_station_serial));
  station_meta_dirty = false;
}

bool TicketSystem::get_station_serial(const String &station,
                                      int &station_serial) {
  return station_serial_tree.find_first_value(station, station_serial);
}

int TicketSystem::get_station_sid(const String &station) {
  int serial = 0;
  if (get_station_serial(station, serial))
    return serial;
  serial = next_station_serial++;
  station_serial_tree.insert(station, serial);
  serial_station_tree.insert(serial, station);
  station_meta_dirty = true;
  return serial;
}

bool TicketSystem::resolve_station_name(int station_serial, String &station) {
  return serial_station_tree.find_first_value(station_serial, station);
}

void TicketSystem::save_waiting_list() {
  save_waiting_meta();
  waiting_dirty = false;
  waiting_dirty_ops = 0;
}

void TicketSystem::mark_waiting_dirty() {
  waiting_dirty = true;
  ++waiting_dirty_ops;
  if (waiting_dirty_ops >= WAITING_SAVE_THRESHOLD) {
    save_waiting_list();
  }
}

int TicketSystem::get_price_sum(const vector<int> &prices, int from_idx,
                                int to_idx) {
  int total = 0;
  for (int i = from_idx; i < to_idx; ++i) {
    total += prices[i];
  }
  return total;
}

bool TicketSystem::add_ticket(const Train &train) {
  if (train.stationNum <= 1)
    return true;
  invalidate_query_caches();
  int train_serial = get_train_sid(train.ID);
  vector<int> all_station_serials;
  for (int i = 0; i < train.stationNum; ++i) {
    all_station_serials.push_back(get_station_sid(train.stations[i]));
    station_pos_tree.insert(all_station_serials.back(),
                            pack_st_pos(train_serial, i));
  }
  TrainMeta train_meta;
  train_meta.from_train(train);
  upsert_train_lite(train_serial, train_meta);

  const int seg_count = train.stationNum - 1;

  for (int from_idx = 0; from_idx < seg_count; ++from_idx) {
    for (int to_idx = from_idx + 1; to_idx < train.stationNum; ++to_idx) {
      DirectPairKey key;
      key.from_station_serial = all_station_serials[from_idx];
      key.to_station_serial = all_station_serials[to_idx];
      DirectPairRef ref;
      ref.train_serial = train_serial;
      ref.from_idx = from_idx;
      ref.to_idx = to_idx;
      direct_ticket_tree.insert(key, pack_pair_ref(ref));
    }
  }
  return true;
}
bool TicketSystem::Compare_with_cost(Ticket &A, Ticket &B) {
  int pa = A.getPrice(), pb = B.getPrice();
  if (pa != pb)
    return pa < pb;
  if (A.trainID != B.trainID)
    return A.trainID < B.trainID;
  if (A.to_station != B.to_station)
    return A.to_station < B.to_station;
  if (A.from_station != B.from_station)
    return A.from_station < B.from_station;
  return A.date < B.date;
}
bool TicketSystem::Compare_with_time(Ticket &A, Ticket &B) {
  int ta = A.getTime(), tb = B.getTime();
  if (ta != tb)
    return ta < tb;
  if (A.trainID != B.trainID)
    return A.trainID < B.trainID;
  if (A.to_station != B.to_station)
    return A.to_station < B.to_station;
  if (A.from_station != B.from_station)
    return A.from_station < B.from_station;
  return A.date < B.date;
}

bool TicketSystem::query_ticket(const String &from_station,
                                const String &to_station, int date,
                                CompareType cmp_type) {
  int from_station_serial = 0;
  if (!get_station_serial(from_station, from_station_serial)) {
    cout << 0 << '\n';
    return true;
  }
  int to_station_serial = 0;
  if (!get_station_serial(to_station, to_station_serial)) {
    cout << 0 << '\n';
    return true;
  }

  QueryTicketCacheKey query_key;
  query_key.from_station_serial = from_station_serial;
  query_key.to_station_serial = to_station_serial;
  query_key.date = date;
  query_key.cmp_type = static_cast<int>(cmp_type);

  DirectPairKey direct_key;
  direct_key.from_station_serial = from_station_serial;
  direct_key.to_station_serial = to_station_serial;

  auto get_train_cache = [&](int train_serial) -> TrainQueryLite * {
    return get_train_lite(train_serial);
  };

  CachedQueryTicketVec final_res;
  bool cache_hit = get_qt_cache(query_key, final_res);
  if (!cache_hit) {
    auto direct_raw = direct_ticket_tree.find(direct_key);
    for (int i = 0; i < direct_raw.size(); ++i) {
      DirectPairRef ref = unpack_pair_ref(direct_raw[i].value);
      TrainQueryLite *cache = get_train_cache(ref.train_serial);
      if (cache == nullptr)
        continue;

      if (ref.from_idx < 0 || ref.to_idx <= ref.from_idx)
        continue;
      if (ref.from_idx >= cache->station_num - 1 ||
          ref.to_idx >= cache->station_num) {
        continue;
      }
      if (ref.from_idx >= static_cast<int>(cache->date.size()))
        continue;

      int depart_day = date - cache->date[ref.from_idx];
      if (depart_day < cache->sale_begin || depart_day > cache->sale_end) {
        continue;
      }

      CachedQueryTicketItem item;
      item.train_serial = ref.train_serial;
      item.depart_day = depart_day;
      item.from_idx = ref.from_idx;
      item.to_idx = ref.to_idx;
      item.price =
          cache->prefix_price[ref.to_idx] - cache->prefix_price[ref.from_idx];
      item.time =
          cache->arrive_time[ref.to_idx] - cache->depart_time[ref.from_idx];
      if (item.time < 0)
        continue;
      final_res.push_back(item);
    }

    if (cmp_type == PRICE) {
      sort_vec(final_res, [&](const CachedQueryTicketItem &A,
                              const CachedQueryTicketItem &B) {
        if (A.price != B.price)
          return A.price < B.price;
        TrainQueryLite *ca = get_train_cache(A.train_serial);
        TrainQueryLite *cb = get_train_cache(B.train_serial);
        if (ca != nullptr && cb != nullptr && ca->train_id != cb->train_id) {
          return ca->train_id < cb->train_id;
        }
        return A.depart_day < B.depart_day;
      });
    } else if (cmp_type == TIME) {
      sort_vec(final_res, [&](const CachedQueryTicketItem &A,
                              const CachedQueryTicketItem &B) {
        if (A.time != B.time)
          return A.time < B.time;
        TrainQueryLite *ca = get_train_cache(A.train_serial);
        TrainQueryLite *cb = get_train_cache(B.train_serial);
        if (ca != nullptr && cb != nullptr && ca->train_id != cb->train_id) {
          return ca->train_id < cb->train_id;
        }
        return A.depart_day < B.depart_day;
      });
    }

    put_qt_cache(query_key, final_res);
  }

  if (final_res.empty()) {
    cout << 0 << '\n';
    return true;
  }

  cout << final_res.size() << '\n';
  hashmap<String, Train> seat_train_cache;
  auto get_train_snapshot = [&](const String &train_id) -> Train & {
    auto it = seat_train_cache.find(train_id);
    if (it == seat_train_cache.end()) {
      auto inserted = seat_train_cache.emplace(train_id, Train());
      it = inserted.first;
      (void)train_system_ptr->find_train(train_id, it->second);
    }
    return it->second;
  };

  hashmap<std::uint64_t, String> time_string_cache;
  auto get_time_cached = [&](int time_value, int day_value) -> const String & {
    std::uint64_t key =
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(day_value))
         << 32) |
        static_cast<std::uint32_t>(time_value);
    auto it = time_string_cache.find(key);
    if (it != time_string_cache.end())
      return it->second;
    auto inserted =
        time_string_cache.emplace(key, real_time(time_value, day_value));
    return inserted.first->second;
  };

  std::string output_buffer;
  output_buffer.reserve(final_res.size() * 96);
  for (auto &item : final_res) {
    TrainQueryLite *cache = get_train_cache(item.train_serial);
    if (cache == nullptr)
      continue;
    const String &depart_ts =
        get_time_cached(cache->depart_time[item.from_idx], item.depart_day);
    const String &arrive_ts =
        get_time_cached(cache->arrive_time[item.to_idx], item.depart_day);

    int seat_num = -1;
    if (split_read_on()) {
      int seat_day_num = -1;
      if (train_system_ptr->get_seat_idx(cache->train_id, item.from_idx,
                                         item.to_idx, item.depart_day,
                                         seat_day_num)) {
        seat_num = seat_day_num;
      } else {
        Train &tr = get_train_snapshot(cache->train_id);
        seat_num = tr.get_seat_idx(item.from_idx, item.to_idx, item.depart_day);
      }
    } else {
      Train &tr = get_train_snapshot(cache->train_id);
      seat_num = tr.get_seat_idx(item.from_idx, item.to_idx, item.depart_day);
    }

    output_buffer.append(cache->train_id.s);
    output_buffer.push_back(' ');
    output_buffer.append(from_station.s);
    output_buffer.push_back(' ');
    output_buffer.append(depart_ts.s);
    output_buffer.append(" -> ");
    output_buffer.append(to_station.s);
    output_buffer.push_back(' ');
    output_buffer.append(arrive_ts.s);
    output_buffer.push_back(' ');
    output_buffer.append(std::to_string(item.price));
    output_buffer.push_back(' ');
    output_buffer.append(std::to_string(seat_num));
    output_buffer.push_back('\n');
  }
  cout << output_buffer;
  return true;
}
bool TicketSystem::query_transfer_ticket(const String &from_station,
                                         const String &to_station, int date,
                                         CompareType cmp_type) {
  struct TransferLeg {
    int train_serial = 0;
    String train_id;
    int from_station_serial = 0;
    int to_station_serial = 0;
    int depart_day = 0;
    int from_idx = -1;
    int to_idx = -1;
    int price = 0;
    int time = 0;
  };

  struct Leg2Opt {
    int train_serial = 0;
    int from_idx = -1;
    int to_idx = -1;
    int depart_offset = 0;
    int arrive_offset = -1;
    int sale_begin = 0;
    int sale_end = -1;
    int price = 0;
    int time = 0;
  };

  struct Leg2Cache {
    vector<Leg2Opt> options;
    int min_second_time = 0x3f3f3f3f;
    int min_second_price = 0x3f3f3f3f;
  };

  int from_station_serial = 0;
  if (!get_station_serial(from_station, from_station_serial)) {
    cout << 0 << '\n';
    return true;
  }
  int to_station_serial = 0;
  if (!get_station_serial(to_station, to_station_serial)) {
    cout << 0 << '\n';
    return true;
  }
  const StationPositionsByTrain *from_pos_map =
      get_pos_cache(from_station_serial);
  if (from_pos_map == nullptr || from_pos_map->empty()) {
    cout << 0 << '\n';
    return true;
  }

  hashmap<int, TrainQueryLite *> train_cache;
  auto get_train_cache = [&](int train_serial) -> TrainQueryLite * {
    auto it = train_cache.find(train_serial);
    if (it != train_cache.end())
      return it->second;
    TrainQueryLite *cache = get_train_lite(train_serial);
    train_cache.emplace(train_serial, cache);
    return cache;
  };

  hashmap<int, String> station_name_cache;
  auto get_st_name = [&](int station_serial, String &station_name) -> bool {
    auto it = station_name_cache.find(station_serial);
    if (it != station_name_cache.end()) {
      station_name = it->second;
      return true;
    }
    String resolved;
    if (!resolve_station_name(station_serial, resolved))
      return false;
    station_name_cache.emplace(station_serial, resolved);
    station_name = resolved;
    return true;
  };

  auto ceil_div_1440 = [](int x) -> int {
    return (x >= 0) ? ((x + 1439) / 1440) : (x / 1440);
  };

  hashmap<int, Leg2Cache> leg2_cache;
  auto get_leg2 = [&](int mid_station_serial) -> const Leg2Cache * {
    auto it = leg2_cache.find(mid_station_serial);
    if (it != leg2_cache.end())
      return &it->second;

    Leg2Cache entry;
    DirectPairKey second_key;
    second_key.from_station_serial = mid_station_serial;
    second_key.to_station_serial = to_station_serial;

    auto second_raw = direct_ticket_tree.find(second_key);
    entry.options.reserve(second_raw.size());
    hashmap<long long, int> best_option_index;
    for (int i = 0; i < second_raw.size(); ++i) {
      DirectPairRef second_ref = unpack_pair_ref(second_raw[i].value);
      TrainQueryLite *second_cache = get_train_cache(second_ref.train_serial);
      if (second_cache == nullptr)
        continue;
      if (second_ref.from_idx < 0 || second_ref.to_idx <= second_ref.from_idx)
        continue;
      if (second_ref.from_idx >= second_cache->station_num - 1 ||
          second_ref.to_idx >= second_cache->station_num) {
        continue;
      }
      if (second_ref.from_idx >=
              static_cast<int>(second_cache->station_serials.size()) ||
          second_ref.to_idx >=
              static_cast<int>(second_cache->station_serials.size())) {
        continue;
      }
      if (second_cache->station_serials[second_ref.from_idx] !=
              mid_station_serial ||
          second_cache->station_serials[second_ref.to_idx] !=
              to_station_serial) {
        continue;
      }

      int depart_offset = second_cache->depart_time[second_ref.from_idx];
      int arrive_offset = second_cache->arrive_time[second_ref.to_idx];
      if (arrive_offset < 0)
        continue;
      int second_time = arrive_offset - depart_offset;
      if (second_time < 0)
        continue;

      Leg2Opt option;
      option.train_serial = second_ref.train_serial;
      option.from_idx = second_ref.from_idx;
      option.to_idx = second_ref.to_idx;
      option.depart_offset = depart_offset;
      option.arrive_offset = arrive_offset;
      option.sale_begin = second_cache->sale_begin;
      option.sale_end = second_cache->sale_end;
      option.price = second_cache->prefix_price[second_ref.to_idx] -
                     second_cache->prefix_price[second_ref.from_idx];
      option.time = second_time;

      long long uniq_key = (static_cast<long long>(option.train_serial) << 32) |
                           static_cast<std::uint32_t>(option.from_idx);
      auto uniq_it = best_option_index.find(uniq_key);
      if (uniq_it == best_option_index.end()) {
        int idx = static_cast<int>(entry.options.size());
        entry.options.push_back(option);
        best_option_index.emplace(uniq_key, idx);
      } else {
        int idx = uniq_it->second;
        if (option.to_idx < entry.options[idx].to_idx) {
          entry.options[idx] = option;
        }
      }
    }

    for (int i = 0; i < static_cast<int>(entry.options.size()); ++i) {
      const Leg2Opt &option = entry.options[i];
      if (option.time < entry.min_second_time) {
        entry.min_second_time = option.time;
      }
      if (option.price < entry.min_second_price) {
        entry.min_second_price = option.price;
      }
    }

    auto inserted = leg2_cache.emplace(mid_station_serial, std::move(entry));
    return &inserted.first->second;
  };

  vector<TicketRef> low_refs;
  for (const auto &kv : *from_pos_map) {
    int train_serial = kv.first;
    TrainQueryLite *cache = get_train_cache(train_serial);
    if (cache == nullptr)
      continue;

    const auto &from_positions = kv.second;
    for (int i = 0; i < static_cast<int>(from_positions.size()); ++i) {
      int from_idx = from_positions[i];
      if (from_idx < 0 || from_idx >= cache->station_num - 1)
        continue;
      if (from_idx >= static_cast<int>(cache->station_serials.size()))
        continue;
      if (cache->station_serials[from_idx] != from_station_serial)
        continue;
      if (from_idx >= static_cast<int>(cache->date.size()))
        continue;

      int depart_day = date - cache->date[from_idx];
      if (depart_day < cache->sale_begin || depart_day > cache->sale_end) {
        continue;
      }

      TicketRef ref;
      ref.train_serial = train_serial;
      ref.depart_day = depart_day;
      ref.from_idx = from_idx;
      low_refs.push_back(ref);
    }
  }
  if (low_refs.empty()) {
    cout << 0 << '\n';
    return true;
  }

  bool has_best = false;
  TransferLeg best_first;
  TransferLeg best_second;
  int best_total_time = 0;
  int best_total_price = 0;

  for (int i = 0; i < low_refs.size(); i++) {
    TicketRef first_ref = low_refs[i];
    TrainQueryLite *first_cache = get_train_cache(first_ref.train_serial);
    if (first_cache == nullptr)
      continue;
    if (first_ref.from_idx < 0 ||
        first_ref.from_idx >= first_cache->station_num - 1)
      continue;
    if (first_ref.from_idx >=
        static_cast<int>(first_cache->station_serials.size()))
      continue;
    if (first_cache->station_serials[first_ref.from_idx] != from_station_serial)
      continue;

    int first_depart_abs = first_ref.depart_day * 1440 +
                           first_cache->depart_time[first_ref.from_idx];

    for (int mid_idx = first_ref.from_idx + 1;
         mid_idx < first_cache->station_num; ++mid_idx) {
      if (mid_idx >= static_cast<int>(first_cache->station_serials.size()))
        continue;
      int mid_station_serial = first_cache->station_serials[mid_idx];
      if (mid_station_serial <= 0)
        continue;
      int first_arrive_offset = first_cache->arrive_time[mid_idx];
      if (first_arrive_offset < 0)
        continue;

      const Leg2Cache *second_entry = get_leg2(mid_station_serial);
      if (second_entry == nullptr || second_entry->options.empty())
        continue;

      int first_arrive_abs = first_ref.depart_day * 1440 + first_arrive_offset;
      int first_price = first_cache->prefix_price[mid_idx] -
                        first_cache->prefix_price[first_ref.from_idx];
      int first_time =
          first_arrive_offset - first_cache->depart_time[first_ref.from_idx];
      if (first_time < 0)
        continue;
      if (has_best && cmp_type == TIME) {
        int lower_bound_time = (first_arrive_abs - first_depart_abs) +
                               second_entry->min_second_time;
        if (lower_bound_time > best_total_time)
          continue;
      }
      if (has_best && cmp_type == PRICE) {
        if (first_price + second_entry->min_second_price > best_total_price)
          continue;
      }

      for (int j = 0; j < static_cast<int>(second_entry->options.size()); ++j) {
        const Leg2Opt &option = second_entry->options[j];
        if (option.train_serial == first_ref.train_serial)
          continue;

        int earliest_depart_day =
            ceil_div_1440(first_arrive_abs - option.depart_offset);
        int second_depart_day = (option.sale_begin > earliest_depart_day)
                                    ? option.sale_begin
                                    : earliest_depart_day;
        if (second_depart_day > option.sale_end)
          continue;

        int second_depart_abs = second_depart_day * 1440 + option.depart_offset;
        if (second_depart_abs < first_arrive_abs)
          continue;

        int second_arrive_abs = second_depart_day * 1440 + option.arrive_offset;
        int cand_total_time = second_arrive_abs - first_depart_abs;
        int cand_total_price = first_price + option.price;

        bool better = false;
        if (!has_best) {
          better = true;
        } else if (cmp_type == PRICE) {
          if (cand_total_price < best_total_price)
            better = true;
          else if (cand_total_price == best_total_price &&
                   cand_total_time < best_total_time) {
            better = true;
          }
        } else {
          if (cand_total_time < best_total_time)
            better = true;
          else if (cand_total_time == best_total_time &&
                   cand_total_price < best_total_price) {
            better = true;
          }
        }

        TrainQueryLite *second_cache = nullptr;
        if (!better && has_best && cand_total_price == best_total_price &&
            cand_total_time == best_total_time) {
          second_cache = get_train_cache(option.train_serial);
          if (second_cache != nullptr &&
              (first_cache->train_id < best_first.train_id ||
               (first_cache->train_id == best_first.train_id &&
                second_cache->train_id < best_second.train_id))) {
            better = true;
          }
        }

        if (!better)
          continue;
        if (second_cache == nullptr) {
          second_cache = get_train_cache(option.train_serial);
          if (second_cache == nullptr)
            continue;
        }

        TransferLeg t1;
        t1.train_serial = first_ref.train_serial;
        t1.train_id = first_cache->train_id;
        t1.from_station_serial = from_station_serial;
        t1.to_station_serial = mid_station_serial;
        t1.depart_day = first_ref.depart_day;
        t1.from_idx = first_ref.from_idx;
        t1.to_idx = mid_idx;
        t1.price = first_price;
        t1.time = first_time;

        TransferLeg t2;
        t2.train_serial = option.train_serial;
        t2.train_id = second_cache->train_id;
        t2.from_station_serial = mid_station_serial;
        t2.to_station_serial = to_station_serial;
        t2.depart_day = second_depart_day;
        t2.from_idx = option.from_idx;
        t2.to_idx = option.to_idx;
        t2.price = option.price;
        t2.time = option.time;

        has_best = true;
        best_first = t1;
        best_second = t2;
        best_total_time = cand_total_time;
        best_total_price = cand_total_price;
      }
    }
  }

  if (!has_best) {
    cout << 0 << '\n';
    return true;
  }

  map<String, Train> transfer_train_cache;
  auto get_transfer_train = [&](const String &train_id) -> Train & {
    auto it = transfer_train_cache.find(train_id);
    if (it == transfer_train_cache.end()) {
      transfer_train_cache[train_id] = Train();
      it = transfer_train_cache.find(train_id);
      (void)train_system_ptr->find_train(train_id, it->second);
    }
    return it->second;
  };
  auto print_leg = [&](const TransferLeg &leg) {
    TrainQueryLite *cache = get_train_cache(leg.train_serial);
    if (cache == nullptr)
      return;

    String from_station_name;
    String to_station_name;
    if (!get_st_name(leg.from_station_serial, from_station_name))
      return;
    if (!get_st_name(leg.to_station_serial, to_station_name))
      return;

    String depart_ts =
        real_time(cache->depart_time[leg.from_idx], leg.depart_day);
    String arrive_ts =
        real_time(cache->arrive_time[leg.to_idx], leg.depart_day);
    int seat_num = -1;
    if (split_read_on()) {
      int seat_day_num = -1;
      if (train_system_ptr->get_seat_idx(leg.train_id, leg.from_idx, leg.to_idx,
                                         leg.depart_day, seat_day_num)) {
        seat_num = seat_day_num;
      } else {
        Train &tr = get_transfer_train(leg.train_id);
        seat_num = tr.get_seat_idx(leg.from_idx, leg.to_idx, leg.depart_day);
      }
    } else {
      Train &tr = get_transfer_train(leg.train_id);
      seat_num = tr.get_seat_idx(leg.from_idx, leg.to_idx, leg.depart_day);
    }
    cout << leg.train_id << ' ' << from_station_name << ' ' << depart_ts
         << " -> " << to_station_name << ' ' << arrive_ts << ' ' << leg.price
         << ' ' << seat_num << '\n';
  };
  print_leg(best_first);
  print_leg(best_second);
  return true;
}
bool TicketSystem::buy_ticket(const TrainMeta &tr_meta, Train *tr_snapshot,
                              Ticket &ticket, int num, bool if_wait,
                              order &result, const String &UserID, int from_idx,
                              int to_idx) {
  if (ticket.date < tr_meta.sale_begin || ticket.date > tr_meta.sale_end)
    return false;
  if (num <= 0 || num > tr_meta.seatNum)
    return false;

  if (from_idx < 0 || to_idx < 0) {
    from_idx = -1;
    to_idx = -1;
    for (int i = 0; i < tr_meta.stationNum; ++i) {
      if (tr_meta.stations[i] == ticket.from_station)
        from_idx = i;
      if (tr_meta.stations[i] == ticket.to_station)
        to_idx = i;
    }
  }
  if (from_idx == -1 || to_idx == -1 || from_idx >= to_idx)
    return false;

  bool split_buy_enabled = split_buy_on();
  bool split_write_enabled = split_write_on();
  bool need_train_snapshot = !(split_buy_enabled && split_write_enabled);
  int train_seat_res = -1;
  if (need_train_snapshot) {
    if (tr_snapshot == nullptr)
      return false;
    train_seat_res = tr_snapshot->get_seat_idx(from_idx, to_idx, ticket.date);
    if (train_seat_res < 0)
      return false;
  }

  int seat_res = train_seat_res;
  if (split_buy_enabled) {
    int day_seat_res = -1;
    bool day_ok = train_system_ptr->get_seat_idx(tr_meta.ID, from_idx, to_idx,
                                                 ticket.date, day_seat_res);
    if (split_write_enabled) {

      if (!day_ok)
        return false;
      seat_res = day_seat_res;
    } else if (day_ok) {

      seat_res =
          (train_seat_res < day_seat_res) ? train_seat_res : day_seat_res;
    }
  }

  int total_price = get_price_sum(tr_meta.prices, from_idx, to_idx);
  ticket.price = total_price;

  if (seat_res < num) {
    if (!if_wait)
      return false;
    else {
      result.ticket = ticket;
      result.num = num;
      result.UserID = UserID;
      result.status = OrderStatus::Pending;
      cout << "queue\n";
      return true;
    }
  } else {
    result.ticket = ticket;
    result.num = num;
    result.UserID = UserID;
    result.status = OrderStatus::Success;
    cout << total_price * num << '\n';
    return true;
  }
}

void TicketSystem::clean_up() {
  invalidate_query_caches();
  train_lite_cache_.clear();
  waiting_list.clear();
  if (std::filesystem::exists(waiting_file)) {
    std::filesystem::remove(waiting_file);
  }
  if (std::filesystem::exists(waiting_meta_file)) {
    std::filesystem::remove(waiting_meta_file);
  }
  if (std::filesystem::exists(serial_meta_file)) {
    std::filesystem::remove(serial_meta_file);
  }
  if (std::filesystem::exists(station_meta_file)) {
    std::filesystem::remove(station_meta_file);
  }
  next_train_serial = 1;
  next_station_serial = 1;
  train_meta_dirty = false;
  station_meta_dirty = false;
  train_serial_tree.clean_up();
  serial_train_tree.clean_up();
  station_serial_tree.clean_up();
  serial_station_tree.clean_up();
  station_train_tree.clean_up();
  station_pos_tree.clean_up();
  waiting_pos_tree.clean_up();
  wait_pos_tree.clean_up();
  waiting_order_store.clean_up();
  ticket_tree.clean_up();
  direct_ticket_tree.clean_up();
}
void TicketSystem::flush_all() {
  save_waiting_list();
  train_serial_tree.flush_all();
  serial_train_tree.flush_all();
  station_serial_tree.flush_all();
  serial_station_tree.flush_all();
  station_train_tree.flush_all();
  station_pos_tree.flush_all();
  if (train_meta_dirty)
    save_serial_meta();
  if (station_meta_dirty)
    save_station_meta();
  waiting_pos_tree.flush_all();
  wait_pos_tree.flush_all();
  ticket_tree.flush_all();
  direct_ticket_tree.flush_all();
}
} // namespace sjtu