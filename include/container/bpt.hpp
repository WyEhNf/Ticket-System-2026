#pragma once
#include "../memoryriver/memoryriver.hpp"
#include "String.hpp"
#include "hashmap.hpp"
#include "list.hpp"
#include "vector.hpp"
#include <ostream>

using namespace std;

namespace sjtu {

struct BptProfileCounters {
  long long cache_hit = 0;
  long long cache_miss = 0;
  long long river_read = 0;
  long long river_update = 0;
  long long evict_total = 0;
  long long evict_dirty = 0;
  long long flush_dirty = 0;
};

inline bool bpt_profile_enabled() { return false; }

inline BptProfileCounters &bpt_profile_counters() {
  static BptProfileCounters counters;
  return counters;
}

inline void bpt_profile_dump_once(std::ostream &os) { (void)os; }

template <typename IndexType, typename ValueType, int ORDER = 16>
class BPlusTree {
public:
  static_assert(
      std::is_trivially_copyable_v<IndexType> ||
          has_member_serializer<IndexType>::value ||
          has_adl_serializer<IndexType>::value,
      "IndexType must be trivially copyable or provide serialize/deserialize");
  static_assert(
      std::is_trivially_copyable_v<ValueType> ||
          has_member_serializer<ValueType>::value ||
          has_adl_serializer<ValueType>::value,
      "ValueType must be trivially copyable or provide serialize/deserialize");
  struct Key {
    IndexType index;
    ValueType value;

    bool operator==(const Key &o) const {
      return index == o.index && value == o.value;
    }

    bool operator<(const Key &o) const {
      if (!(index == o.index))
        return index < o.index;
      return value < o.value;
    }

    void serialize(std::ostream &os) const {
      Serializer<IndexType>::write(os, index);
      Serializer<ValueType>::write(os, value);
    }
    void deserialize(std::istream &is) {
      Serializer<IndexType>::read(is, index);
      Serializer<ValueType>::read(is, value);
    }
  };

private:
  struct Node {
    bool is_leaf = false;
    int key_cnt = 0;
    int parent = -1;
    int next = -1;
    Key keys[ORDER];
    int child[ORDER + 1];

    Node() {

      for (int i = 0; i < ORDER + 1; ++i)
        child[i] = -1;
    }

    void serialize(std::ostream &os) const {
      os.write(reinterpret_cast<const char *>(&is_leaf), sizeof(is_leaf));
      os.write(reinterpret_cast<const char *>(&key_cnt), sizeof(key_cnt));
      os.write(reinterpret_cast<const char *>(&parent), sizeof(parent));
      os.write(reinterpret_cast<const char *>(&next), sizeof(next));

      for (int i = 0; i < ORDER; ++i)
        keys[i].serialize(os);

      os.write(reinterpret_cast<const char *>(child),
               sizeof(child[0]) * (ORDER + 1));
    }

    void deserialize(std::istream &is) {
      is.read(reinterpret_cast<char *>(&is_leaf), sizeof(is_leaf));
      is.read(reinterpret_cast<char *>(&key_cnt), sizeof(key_cnt));
      is.read(reinterpret_cast<char *>(&parent), sizeof(parent));
      is.read(reinterpret_cast<char *>(&next), sizeof(next));
      for (int i = 0; i < ORDER; ++i)
        keys[i].deserialize(is);
      is.read(reinterpret_cast<char *>(child), sizeof(child[0]) * (ORDER + 1));
    }
  };

  MemoryRiver<Node, 2> river;

  struct CacheBlock {
    Node node;
    bool dirty = false;
    int pos;
  };

#ifndef SJTU_BPT_CACHE_CAPACITY
#define SJTU_BPT_CACHE_CAPACITY 32
#endif
  int cache_capacity_ = SJTU_BPT_CACHE_CAPACITY;
  int protected_capacity_ =
      (SJTU_BPT_CACHE_CAPACITY >= 4) ? (SJTU_BPT_CACHE_CAPACITY * 3 / 4) : 1;

  using CacheIter = typename list<CacheBlock>::iterator;
  struct CacheRef {
    bool in_protected = false;
    CacheIter it;
  };

  list<CacheBlock> probation_list;
  list<CacheBlock> protected_list;
  hashmap<int, CacheRef> cache_map;
  bool root_pinned_valid = false;
  CacheBlock root_pinned_block;

  std::size_t cache_size() const {
    return probation_list.size() + protected_list.size();
  }

  void rebalance_protected() {
    while (protected_list.size() >
           static_cast<std::size_t>(protected_capacity_)) {
      auto demote_it = protected_list.end();
      --demote_it;
      probation_list.splice(probation_list.begin(), protected_list, demote_it);
      cache_map[probation_list.begin()->pos] =
          CacheRef{false, probation_list.begin()};
    }
  }

  void evict_one_cache_block() {
    if (cache_size() < static_cast<std::size_t>(cache_capacity_))
      return;

    bool use_probation = !probation_list.empty();
    auto &victim_list = use_probation ? probation_list : protected_list;
    auto victim_it = victim_list.end();
    --victim_it;
    CacheBlock &victim = *victim_it;
    if (bpt_profile_enabled()) {
      auto &p = bpt_profile_counters();
      ++p.evict_total;
      if (victim.dirty)
        ++p.evict_dirty;
    }
    if (victim.dirty) {
      if (bpt_profile_enabled())
        ++bpt_profile_counters().river_update;
      river.update(victim.node, victim.pos);
    }
    cache_map.erase(victim.pos);
    victim_list.erase(victim_it);
  }

  void add_cache_blk(const Node &x, bool dirty, int pos) {
    evict_one_cache_block();
    probation_list.push_front({x, dirty, pos});
    cache_map[pos] = CacheRef{false, probation_list.begin()};
  }

  void touch_cache_ref(CacheRef &ref, bool make_dirty) {
    if (ref.in_protected) {
      protected_list.splice(protected_list.begin(), protected_list, ref.it);
      ref.it = protected_list.begin();
    } else {
      protected_list.splice(protected_list.begin(), probation_list, ref.it);
      ref.in_protected = true;
      ref.it = protected_list.begin();
      rebalance_protected();
    }
    if (make_dirty)
      ref.it->dirty = true;
  }

  void flush_root_pin() {
    if (!root_pinned_valid || !root_pinned_block.dirty)
      return;
    if (bpt_profile_enabled()) {
      auto &p = bpt_profile_counters();
      ++p.flush_dirty;
      ++p.river_update;
    }
    river.update(root_pinned_block.node, root_pinned_block.pos);
    root_pinned_block.dirty = false;
  }

  void pin_root_pos(int root_pos) {
    if (root_pinned_valid && root_pinned_block.pos == root_pos)
      return;
    flush_root_pin();

    root_pinned_valid = false;
    auto map_it = cache_map.find(root_pos);
    if (map_it != cache_map.end()) {
      CacheRef ref = map_it->second;
      root_pinned_block = *(ref.it);
      if (ref.in_protected) {
        protected_list.erase(ref.it);
      } else {
        probation_list.erase(ref.it);
      }
      cache_map.erase(map_it);
      root_pinned_valid = true;
      return;
    }

    if (bpt_profile_enabled()) {
      auto &p = bpt_profile_counters();
      ++p.cache_miss;
      ++p.river_read;
    }
    river.read(root_pinned_block.node, root_pos);
    root_pinned_block.pos = root_pos;
    root_pinned_block.dirty = false;
    root_pinned_valid = true;
  }

  Node node(int pos) {
    if (root_pinned_valid && root_pinned_block.pos == pos) {
      if (bpt_profile_enabled())
        ++bpt_profile_counters().cache_hit;
      return root_pinned_block.node;
    }
    auto map_it = cache_map.find(pos);
    if (map_it != cache_map.end()) {
      if (bpt_profile_enabled())
        ++bpt_profile_counters().cache_hit;
      touch_cache_ref(map_it->second, false);
      return map_it->second.it->node;
    }
    if (bpt_profile_enabled()) {
      auto &p = bpt_profile_counters();
      ++p.cache_miss;
      ++p.river_read;
    }
    Node x;
    river.read(x, pos);
    add_cache_blk(x, false, pos);
    return x;
  }

  const Node &node_ref(int pos) {
    if (root_pinned_valid && root_pinned_block.pos == pos) {
      if (bpt_profile_enabled())
        ++bpt_profile_counters().cache_hit;
      return root_pinned_block.node;
    }
    auto map_it = cache_map.find(pos);
    if (map_it != cache_map.end()) {
      if (bpt_profile_enabled())
        ++bpt_profile_counters().cache_hit;
      touch_cache_ref(map_it->second, false);
      return map_it->second.it->node;
    }
    if (bpt_profile_enabled()) {
      auto &p = bpt_profile_counters();
      ++p.cache_miss;
      ++p.river_read;
    }
    Node x;
    river.read(x, pos);
    add_cache_blk(x, false, pos);
    auto inserted_it = cache_map.find(pos);
    return inserted_it->second.it->node;
  }

  Node &node_mut(int pos) {
    if (root_pinned_valid && root_pinned_block.pos == pos) {
      if (bpt_profile_enabled())
        ++bpt_profile_counters().cache_hit;
      root_pinned_block.dirty = true;
      return root_pinned_block.node;
    }
    auto map_it = cache_map.find(pos);
    if (map_it != cache_map.end()) {
      if (bpt_profile_enabled())
        ++bpt_profile_counters().cache_hit;
      touch_cache_ref(map_it->second, true);
      return map_it->second.it->node;
    }
    if (bpt_profile_enabled()) {
      auto &p = bpt_profile_counters();
      ++p.cache_miss;
      ++p.river_read;
    }
    Node x;
    river.read(x, pos);
    add_cache_blk(x, true, pos);
    auto inserted_it = cache_map.find(pos);
    return inserted_it->second.it->node;
  }

  void write_node(const Node &x, int pos) {
    if (root_pinned_valid && root_pinned_block.pos == pos) {
      if (bpt_profile_enabled())
        ++bpt_profile_counters().cache_hit;
      root_pinned_block.node = x;
      root_pinned_block.dirty = true;
      return;
    }
    auto map_it = cache_map.find(pos);
    if (map_it != cache_map.end()) {
      if (bpt_profile_enabled())
        ++bpt_profile_counters().cache_hit;
      map_it->second.it->node = x;
      touch_cache_ref(map_it->second, true);
    } else {
      if (bpt_profile_enabled())
        ++bpt_profile_counters().cache_miss;
      add_cache_blk(x, true, pos);
    }
  }

  Node add_to_cache(const Node &x, int pos) {
    add_cache_blk(x, false, pos);
    return x;
  }

public:
  void flush_all() {
    flush_root_pin();
    for (auto &block : probation_list) {
      if (block.dirty) {
        if (bpt_profile_enabled()) {
          auto &p = bpt_profile_counters();
          ++p.flush_dirty;
          ++p.river_update;
        }
        river.update(block.node, block.pos);
        block.dirty = false;
      }
    }
    for (auto &block : protected_list) {
      if (block.dirty) {
        if (bpt_profile_enabled()) {
          auto &p = bpt_profile_counters();
          ++p.flush_dirty;
          ++p.river_update;
        }
        river.update(block.node, block.pos);
        block.dirty = false;
      }
    }
  }

private:
  int min_leaf_keys() const { return (ORDER + 1) / 2; }
  int min_internal_keys() const { return (ORDER - 1) / 2; }

  int child_idx_ub(const Node &x, const Key &key) const {
    int l = 0;
    int r = x.key_cnt;
    while (l < r) {
      int mid = (l + r) >> 1;
      if (key < x.keys[mid]) {
        r = mid;
      } else {
        l = mid + 1;
      }
    }
    return l;
  }

  int child_index_linear(const Node &par, int child_pos) const {
    for (int i = 0; i <= par.key_cnt; ++i) {
      if (par.child[i] == child_pos)
        return i;
    }
    return -1;
  }

  int child_idx_key(const Node &par, int child_pos,
                    const Key &first_key) const {
    int idx = child_idx_ub(par, first_key);
    if (idx < 0)
      idx = 0;
    if (idx > par.key_cnt)
      idx = par.key_cnt;
    if (par.child[idx] == child_pos)
      return idx;
    if (idx > 0 && par.child[idx - 1] == child_pos)
      return idx - 1;
    if (idx < par.key_cnt && par.child[idx + 1] == child_pos)
      return idx + 1;
    return child_index_linear(par, child_pos);
  }

  int root() {
    int r;
    river.get_info(r, 1);
    pin_root_pos(r);
    return r;
  }
  void set_root(int r) {
    river.write_info(r, 1);
    pin_root_pos(r);
  }

  int new_node(const Node &x) {
    int pos = river.write(x);
    add_to_cache(x, pos);
    int cnt;
    river.get_info(cnt, 2);
    river.write_info(cnt + 1, 2);
    return pos;
  }

  int find_leaf(const Key &key) {
    int u = root();
    while (true) {
      const Node &x = node_ref(u);
      if (x.is_leaf)
        return u;
      int i = child_idx_ub(x, key);
      u = x.child[i];
    }
  }

  void insert_in_leaf(int u, const Key &key) {
    Node &x = node_mut(u);
    int i = x.key_cnt;
    while (i > 0 && key < x.keys[i - 1]) {
      x.keys[i] = x.keys[i - 1];
      i--;
    }
    x.keys[i] = key;
    x.key_cnt++;
    if (x.key_cnt == ORDER) {
      split_leaf(u);
    }
  }

  void split_leaf(int u) {
    Node x = node(u), y;
    y.is_leaf = true;
    y.parent = x.parent;

    int mid = ORDER / 2;
    y.key_cnt = x.key_cnt - mid;
    for (int i = 0; i < y.key_cnt; i++) {
      y.keys[i] = x.keys[mid + i];
    }

    x.key_cnt = mid;
    y.next = x.next;
    x.next = new_node(y);
    write_node(x, u);
    write_node(y, x.next);

    insert_in_parent(u, y.keys[0], x.next);
  }

  void insert_in_parent(int u, const Key &key, int v) {
    if (u == root()) {
      Node r;
      r.is_leaf = false;
      r.key_cnt = 1;
      r.keys[0] = key;
      r.child[0] = u;
      r.child[1] = v;
      int rp = new_node(r);

      Node cu = node(u);
      cu.parent = rp;
      write_node(cu, u);
      Node cv = node(v);
      cv.parent = rp;
      write_node(cv, v);

      set_root(rp);
      return;
    }

    Node cur = node(u);
    int p = cur.parent;
    Node par = node(p);

    int i = par.key_cnt;
    while (i > 0 && key < par.keys[i - 1]) {
      par.keys[i] = par.keys[i - 1];
      par.child[i + 1] = par.child[i];
      i--;
    }
    par.keys[i] = key;
    par.child[i + 1] = v;
    par.key_cnt++;

    Node cv = node(v);
    cv.parent = p;
    write_node(cv, v);
    write_node(par, p);

    if (par.key_cnt == ORDER)
      split_internal(p);
  }

  void split_internal(int u) {
    Node x = node(u), y;
    y.is_leaf = false;
    y.parent = x.parent;

    int mid = ORDER / 2;
    Key up = x.keys[mid];

    y.key_cnt = x.key_cnt - mid - 1;
    for (int i = 0; i < y.key_cnt; i++) {
      y.keys[i] = x.keys[mid + 1 + i];
    }

    for (int i = 0; i <= y.key_cnt; i++) {
      y.child[i] = x.child[mid + 1 + i];
    }

    x.key_cnt = mid;
    int v = new_node(y);
    for (int i = 0; i <= y.key_cnt; i++) {
      Node c = node(y.child[i]);
      c.parent = v;
      write_node(c, y.child[i]);
    }

    write_node(x, u);
    write_node(y, v);
    insert_in_parent(u, up, v);
  }

  void fix_leaf(int u) {
    Node cur = node(u);
    int p = cur.parent;
    Node par = node(p);
    int idx = 0;
    while (par.child[idx] != u)
      idx++;

    if (idx > 0) {
      int l = par.child[idx - 1];
      Node left = node(l);
      if (left.key_cnt > min_leaf_keys()) {
        for (int i = cur.key_cnt; i > 0; i--)
          cur.keys[i] = cur.keys[i - 1];
        cur.keys[0] = left.keys[left.key_cnt - 1];
        cur.key_cnt++;
        left.key_cnt--;
        par.keys[idx - 1] = cur.keys[0];
        write_node(left, l);
        write_node(cur, u);
        write_node(par, p);
        return;
      }
    }

    if (idx + 1 <= par.key_cnt) {
      int r = par.child[idx + 1];
      Node right = node(r);
      if (right.key_cnt > min_leaf_keys()) {
        cur.keys[cur.key_cnt++] = right.keys[0];
        for (int i = 0; i + 1 < right.key_cnt; i++)
          right.keys[i] = right.keys[i + 1];
        right.key_cnt--;
        par.keys[idx] = right.keys[0];
        write_node(right, r);
        write_node(cur, u);
        write_node(par, p);
        return;
      }
    }

    if (idx > 0)
      merge_leaf(par.child[idx - 1], u, idx - 1);
    else
      merge_leaf(u, par.child[idx + 1], idx);
  }

  void merge_leaf(int l, int r, int sep) {
    Node left = node(l);
    Node right = node(r);
    Node par = node(left.parent);

    for (int i = 0; i < right.key_cnt; ++i)
      left.keys[left.key_cnt + i] = right.keys[i];
    left.key_cnt += right.key_cnt;
    left.next = right.next;
    for (int i = sep; i + 1 < par.key_cnt; ++i) {
      par.keys[i] = par.keys[i + 1];
      par.child[i + 1] = par.child[i + 2];
    }
    par.key_cnt--;

    write_node(left, l);
    write_node(par, left.parent);

    if (par.parent != -1 && par.key_cnt < min_internal_keys())
      fix_internal(left.parent);

    if (par.key_cnt == 0 && left.parent == root()) {
      set_root(l);
      left.parent = -1;
      write_node(left, l);
    }
  }

  void fix_internal(int u) {
    Node x = node(u);
    if (x.parent == -1 || x.key_cnt >= min_internal_keys())
      return;

    Node par = node(x.parent);
    int idx = 0;
    while (par.child[idx] != u)
      idx++;

    if (idx > 0) {
      int l = par.child[idx - 1];
      Node left = node(l);
      if (left.key_cnt > min_internal_keys()) {
        for (int i = x.key_cnt; i > 0; i--)
          x.keys[i] = x.keys[i - 1];
        for (int i = x.key_cnt + 1; i > 0; i--)
          x.child[i] = x.child[i - 1];
        x.keys[0] = par.keys[idx - 1];
        x.child[0] = left.child[left.key_cnt];
        Node c = node(x.child[0]);
        c.parent = u;
        write_node(c, x.child[0]);
        par.keys[idx - 1] = left.keys[left.key_cnt - 1];
        x.key_cnt++;
        left.key_cnt--;
        write_node(left, l);
        write_node(x, u);
        write_node(par, x.parent);
        return;
      }
    }

    if (idx < par.key_cnt) {
      int r = par.child[idx + 1];
      Node right = node(r);
      if (right.key_cnt > min_internal_keys()) {
        x.keys[x.key_cnt] = par.keys[idx];
        x.child[x.key_cnt + 1] = right.child[0];
        Node c = node(x.child[x.key_cnt + 1]);
        c.parent = u;
        write_node(c, x.child[x.key_cnt + 1]);
        par.keys[idx] = right.keys[0];
        for (int i = 0; i + 1 < right.key_cnt; i++)
          right.keys[i] = right.keys[i + 1];
        for (int i = 0; i + 1 <= right.key_cnt; i++)
          right.child[i] = right.child[i + 1];
        x.key_cnt++;
        right.key_cnt--;
        write_node(right, r);
        write_node(x, u);
        write_node(par, x.parent);
        return;
      }
    }
    if (idx > 0)
      merge_internal(par.child[idx - 1], u, idx - 1);
    else
      merge_internal(u, par.child[idx + 1], idx);
  }

  void merge_internal(int l, int r, int sep) {
    Node left = node(l);
    Node right = node(r);
    Node par = node(left.parent);

    left.keys[left.key_cnt++] = par.keys[sep];
    for (int i = 0; i < right.key_cnt; i++) {
      left.keys[left.key_cnt + i] = right.keys[i];
    }

    for (int i = 0; i <= right.key_cnt; i++) {
      left.child[left.key_cnt + i] = right.child[i];
      Node c = node(right.child[i]);
      c.parent = l;
      write_node(c, right.child[i]);
    }

    left.key_cnt += right.key_cnt;

    for (int i = sep; i + 1 < par.key_cnt; i++) {
      par.keys[i] = par.keys[i + 1];
      par.child[i + 1] = par.child[i + 2];
    }
    par.key_cnt--;

    write_node(left, l);
    write_node(par, left.parent);

    if (par.parent == -1 && par.key_cnt == 0) {
      set_root(l);
      left.parent = -1;
      write_node(left, l);
      return;
    }

    if (par.parent != -1 && par.key_cnt < min_internal_keys()) {
      fix_internal(left.parent);
    }
  }

public:
  BPlusTree(string fn = "bpt.db",
            int cache_capacity = SJTU_BPT_CACHE_CAPACITY) {
    cache_capacity_ = (cache_capacity >= 4) ? cache_capacity : 4;
    protected_capacity_ =
        (cache_capacity_ >= 4) ? (cache_capacity_ * 3 / 4) : 1;
    river.initialise(fn);
    int cnt;
    river.get_info(cnt, 2);
    if (cnt == 0) {
      Node r;
      r.is_leaf = true;
      int rp = river.write(r);
      river.write_info(rp, 1);
      river.write_info(1, 2);
    }
  }

  ~BPlusTree() { flush_all(); }

  void insert(const IndexType &idx, const ValueType &val) {
    Key key{};
    key.index = idx;
    key.value = val;
    int u = find_leaf(key);
    const Node &x = node_ref(u);
    for (int i = 0; i < x.key_cnt; i++)
      if (x.keys[i] == key)
        return;
    insert_in_leaf(u, key);
  }

  void erase(const IndexType &idx, const ValueType &val) {
    Key key{};
    key.index = idx;
    key.value = val;
    int u = find_leaf(key);
    Node &x = node_mut(u);
    int pos = -1;
    for (int i = 0; i < x.key_cnt; i++) {
      if (x.keys[i] == key) {
        pos = i;
        break;
      }
    }
    if (pos == -1)
      return;
    for (int i = pos; i + 1 < x.key_cnt; i++)
      x.keys[i] = x.keys[i + 1];
    x.key_cnt--;
    if (u != root() && x.key_cnt < min_leaf_keys())
      fix_leaf(u);
  }

  bool update(const IndexType &idx, const ValueType &old_val,
              const ValueType &new_val) {
    Key target_key{};
    target_key.index = idx;
    target_key.value = old_val;

    int u = find_leaf(target_key);
    const Node &x = node_ref(u);
    for (int i = 0; i < x.key_cnt; i++) {
      if (x.keys[i] == target_key) {
        Node &xm = node_mut(u);
        xm.keys[i].value = new_val;
        return true;
      }
    }
    return false;
  }

  bool update_first_value(const IndexType &idx, const ValueType &new_val) {
    Key low{};
    low.index = idx;
    if constexpr (std::is_same_v<ValueType, String>) {
      low.value = String::min_value();
    } else {
      low.value = ValueType{};
    }

    int u = find_leaf(low);
    while (u != -1) {
      const Node &x = node_ref(u);
      for (int i = 0; i < x.key_cnt; ++i) {
        if (x.keys[i].index == idx) {
          Node &xm = node_mut(u);
          xm.keys[i].value = new_val;
          return true;
        }
        if (x.keys[i].index > idx)
          return false;
      }
      u = x.next;
    }
    return false;
  }

  vector<Key> find(const IndexType &idx) {
    Key low{};
    low.index = idx;
    if constexpr (std::is_same_v<ValueType, String>) {
      low.value = String::min_value();
    } else {
      low.value = ValueType{};
    }
    int u = find_leaf(low);
    vector<Key> result;
    while (u != -1) {
      const Node &x = node_ref(u);
      bool found = false;
      for (int i = 0; i < x.key_cnt; i++) {
        if (x.keys[i].index == low.index) {
          result.push_back(x.keys[i]);
          found = true;
        } else if (x.keys[i].index > low.index) {
          return result;
        }
      }
      u = x.next;
    }
    return result;
  }

  template <typename Func> void for_each_key(const IndexType &idx, Func fn) {
    Key low{};
    low.index = idx;
    if constexpr (std::is_same_v<ValueType, String>) {
      low.value = String::min_value();
    } else {
      low.value = ValueType{};
    }

    int u = find_leaf(low);
    while (u != -1) {
      const Node &x = node_ref(u);
      for (int i = 0; i < x.key_cnt; ++i) {
        if (x.keys[i].index == idx) {
          fn(x.keys[i]);
        } else if (x.keys[i].index > idx) {
          return;
        }
      }
      u = x.next;
    }
  }

  bool find_first_key(const IndexType &idx, Key &out_key) {
    Key low{};
    low.index = idx;
    if constexpr (std::is_same_v<ValueType, String>) {
      low.value = String::min_value();
    } else {
      low.value = ValueType{};
    }

    int u = find_leaf(low);
    while (u != -1) {
      const Node &x = node_ref(u);
      for (int i = 0; i < x.key_cnt; ++i) {
        if (x.keys[i].index == idx) {
          out_key = x.keys[i];
          return true;
        }
        if (x.keys[i].index > idx)
          return false;
      }
      u = x.next;
    }
    return false;
  }

  bool find_first_value(const IndexType &idx, ValueType &out_value) {
    Key out_key{};
    if (!find_first_key(idx, out_key))
      return false;
    out_value = out_key.value;
    return true;
  }

  bool contains_index(const IndexType &idx) {
    Key out_key{};
    return find_first_key(idx, out_key);
  }
  void clean_up() {
    probation_list.clear();
    protected_list.clear();
    cache_map.clear();
    root_pinned_valid = false;

    river.clean_up();
    Node r;
    r.is_leaf = true;
    r.parent = -1;
    r.key_cnt = 0;

    int rp = river.write(r);
    river.write_info(rp, 1);
    river.write_info(1, 2);
    add_to_cache(r, rp);
  }
};

} // namespace sjtu