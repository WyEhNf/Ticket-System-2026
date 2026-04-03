#include "../../include/system/TrainSystem.hpp"
using namespace std;
namespace sjtu {

void TrainSystem::touch_day_cache(
    hashmap<SeatKey, SeatDayCacheEntry>::iterator it) {
  seat_day_lru_.splice(seat_day_lru_.begin(), seat_day_lru_, it->second.lru_it);
}

void TrainSystem::evict_day_cache() {
  if (seat_day_lru_.empty())
    return;
  SeatKey evict_key = seat_day_lru_.back();
  seat_day_lru_.pop_back();
  auto it = seat_day_cache_.find(evict_key);
  if (it == seat_day_cache_.end())
    return;
  if (it->second.dirty && it->second.seat_pos >= 0) {
    seat_store.update(it->second.day, it->second.seat_pos);
    it->second.dirty = false;
  }
  seat_day_cache_.erase(it);
}

void TrainSystem::flush_day_cache() {
  for (auto it = seat_day_cache_.begin(); it != seat_day_cache_.end(); ++it) {
    if (it->second.dirty && it->second.seat_pos >= 0) {
      seat_store.update(it->second.day, it->second.seat_pos);
      it->second.dirty = false;
    }
  }
}

bool TrainSystem::load_day_data(const SeatKey &key, SeatDay &out_day,
                                int &out_pos) {
  auto it = seat_day_cache_.find(key);
  if (it != seat_day_cache_.end()) {
    touch_day_cache(it);
    out_day = it->second.day;
    out_pos = it->second.seat_pos;
    return true;
  }

  int seat_pos = 0;
  if (!seat_tree.find_first_value(key, seat_pos))
    return false;

  SeatDay day;
  seat_store.read(day, seat_pos);
  cache_day_data(key, day, seat_pos, false);
  out_day = day;
  out_pos = seat_pos;
  return true;
}

void TrainSystem::cache_day_data(const SeatKey &key, const SeatDay &day,
                                 int pos, bool dirty) {
  auto it = seat_day_cache_.find(key);
  if (it != seat_day_cache_.end()) {
    it->second.day = day;
    it->second.seat_pos = pos;
    it->second.dirty = it->second.dirty || dirty;
    touch_day_cache(it);
    return;
  }

  if (seat_day_cache_.size() >= SEAT_DAY_CACHE_LIMIT) {
    evict_day_cache();
  }

  seat_day_lru_.push_front(key);
  SeatDayCacheEntry entry;
  entry.day = day;
  entry.seat_pos = pos;
  entry.dirty = dirty;
  entry.lru_it = seat_day_lru_.begin();
  seat_day_cache_.emplace(key, std::move(entry));
}

bool TrainSystem::ensure_seat_day(const String &train_id, int date,
                                  SeatDay &out_day) {
  SeatKey key{train_id, date};
  int seat_pos = -1;
  if (load_day_data(key, out_day, seat_pos)) {
    return true;
  }

  TrainMeta meta;
  if (!find_train_meta(train_id, meta) || meta.ID == String() || !meta.released)
    return false;
  if (date < meta.sale_begin || date > meta.sale_end)
    return false;

  SeatDay seeded;
  seeded.station_num = meta.stationNum;
  for (int i = 0; i < meta.stationNum - 1; ++i) {
    seeded.seg_seat[i] = meta.seatNum;
  }
  int new_pos = seat_store.write(seeded);
  seat_tree.insert(key, new_pos);
  cache_day_data(key, seeded, new_pos, false);
  out_day = seeded;
  return true;
}

bool TrainSystem::write_seat_day(const String &train_id, int date,
                                 const SeatDay &old_day,
                                 const SeatDay &new_day) {
  SeatKey key{train_id, date};
  SeatDay persisted_day;
  int seat_pos = -1;
  if (load_day_data(key, persisted_day, seat_pos)) {
    cache_day_data(key, new_day, seat_pos, true);
    return true;
  }

  int new_pos = seat_store.write(new_day);
  seat_tree.insert(key, new_pos);
  cache_day_data(key, new_day, new_pos, false);
  return true;
}

bool TrainSystem::add_train(const Train &new_train) {
  if (train_tree.contains_index(new_train.ID))
    return false;
  int train_pos = train_store.write(new_train);
  train_tree.insert(new_train.ID, train_pos);
  TrainMeta meta;
  meta.from_train(new_train);
  int meta_pos = train_meta_store.write(meta);
  train_meta_tree.insert(new_train.ID, meta_pos);
  return true;
}
bool TrainSystem::delete_train(const String &train_id) {
  int train_pos = 0;
  if (!train_tree.find_first_value(train_id, train_pos))
    return false;
  Train old_train;
  train_store.read(old_train, train_pos);
  if (old_train.released)
    return false;
  train_tree.erase(train_id, train_pos);
  int meta_pos = 0;
  if (train_meta_tree.find_first_value(train_id, meta_pos)) {
    train_meta_tree.erase(train_id, meta_pos);
  }
  return true;
}
bool TrainSystem::find_train(const String &train_id, Train &out_train) {
  int train_pos = 0;
  if (!train_tree.find_first_value(train_id, train_pos))
    return false;
  train_store.read(out_train, train_pos);
  return true;
}
bool TrainSystem::find_train_meta(const String &train_id, TrainMeta &out_meta) {
  int meta_pos = 0;
  if (!train_meta_tree.find_first_value(train_id, meta_pos))
    return false;
  train_meta_store.read(out_meta, meta_pos);
  return true;
}
bool TrainSystem::get_seat_day(const String &train_id, int date,
                               SeatDay &out_day) {
  SeatKey key{train_id, date};
  int seat_pos = -1;
  if (load_day_data(key, out_day, seat_pos)) {
    return true;
  }

  TrainMeta meta;
  if (!find_train_meta(train_id, meta) || meta.ID == String() || !meta.released)
    return false;
  if (date < meta.sale_begin || date > meta.sale_end)
    return false;

  out_day.station_num = meta.stationNum;
  for (int i = 0; i < meta.stationNum - 1; ++i) {
    out_day.seg_seat[i] = meta.seatNum;
  }
  return true;
}
bool TrainSystem::get_seat_idx(const String &train_id, int from, int to,
                               int date, int &result) {
  result = -1;
  if (from < 0 || to < 0 || from >= to)
    return false;

  SeatDay day;
  if (!get_seat_day(train_id, date, day))
    return false;
  if (day.station_num <= 0)
    return false;
  if (to > day.station_num)
    return false;

  int min_seat = day.seg_seat[from];
  for (int i = from; i < to; ++i) {
    if (day.seg_seat[i] < min_seat)
      min_seat = day.seg_seat[i];
  }
  result = min_seat;
  return true;
}

bool TrainSystem::set_seat_idx(const String &train_id, int from, int to,
                               int date, int delta) {
  if (from < 0 || to < 0 || from >= to)
    return false;

  SeatDay old_day;
  if (!ensure_seat_day(train_id, date, old_day))
    return false;
  SeatDay new_day = old_day;
  if (new_day.station_num <= 0 || to > new_day.station_num)
    return false;
  for (int i = from; i < to; ++i) {
    new_day.seg_seat[i] -= delta;
  }
  return write_seat_day(train_id, date, old_day, new_day);
}

bool TrainSystem::update_train_snapshot(const String &train_id,
                                        const Train &old_train,
                                        const Train &new_train) {
  int train_pos = 0;
  if (!train_tree.find_first_value(train_id, train_pos))
    return false;
  train_store.update(new_train, train_pos);
  return true;
}

bool TrainSystem::update_train_snapshot(const String &train_id,
                                        const Train &new_train) {
  int train_pos = 0;
  if (!train_tree.find_first_value(train_id, train_pos))
    return false;
  train_store.update(new_train, train_pos);
  return true;
}

bool TrainSystem::release_train(const String &train_id, Train *released_train) {
  int train_pos = 0;
  if (!train_tree.find_first_value(train_id, train_pos))
    return false;
  Train t;
  train_store.read(t, train_pos);
  if (t.released)
    return false;
  t.released = true;
  t.ensure_seat_res();
  const int active_days = t.sale_end - t.sale_begin + 1;
  const int seg_count = t.stationNum - 1;
  const int stride = seg_count;
  for (int day = 0; day < active_days; ++day) {
    int *day_base = t.seat_res + day * stride;
    std::fill_n(day_base, seg_count, t.seatNum);
  }

  t.initialise();
  train_store.update(t, train_pos);

  TrainMeta new_meta;
  new_meta.from_train(t);
  int meta_pos = 0;
  if (train_meta_tree.find_first_value(t.ID, meta_pos)) {
    train_meta_store.update(new_meta, meta_pos);
  } else {
    int new_meta_pos = train_meta_store.write(new_meta);
    train_meta_tree.insert(t.ID, new_meta_pos);
  }
  if (released_train != nullptr) {
    *released_train = std::move(t);
  }
  return true;
}
void TrainSystem::clean_up() {
  seat_day_cache_.clear();
  seat_day_lru_.clear();
  train_tree.clean_up();
  train_meta_tree.clean_up();
  seat_tree.clean_up();
  train_store.clean_up();
  train_meta_store.clean_up();
  seat_store.clean_up();
}
void TrainSystem::flush_all() {
  flush_day_cache();
  train_tree.flush_all();
  train_meta_tree.flush_all();
  seat_tree.flush_all();
}

} // namespace sjtu