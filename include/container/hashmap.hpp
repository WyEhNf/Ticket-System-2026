#ifndef SJTU_HASHMAP_HPP
#define SJTU_HASHMAP_HPP

#include <cstddef>
#include <type_traits>
#include <utility>

#include "utility.hpp"

namespace sjtu {

inline std::size_t hash_mix64(std::size_t x) {
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebULL;
  x ^= x >> 31;
  return x;
}

template <typename Key, typename Enable = void> struct default_hash;

template <typename Key>
struct default_hash<
    Key, std::enable_if_t<std::is_integral_v<Key> || std::is_enum_v<Key>>> {
  std::size_t operator()(const Key &key) const {
    return hash_mix64(static_cast<std::size_t>(key));
  }
};

template <typename Key>
struct default_hash<
    Key, std::enable_if_t<!std::is_integral_v<Key> && !std::is_enum_v<Key>>> {
  std::size_t operator()(const Key &key) const { return key.hash(); }
};

template <typename Key> struct default_equal {
  bool operator()(const Key &a, const Key &b) const { return a == b; }
};

template <typename Key, typename T, typename Hash = default_hash<Key>,
          typename KeyEqual = default_equal<Key>>
class hashmap {
public:
  using value_type = pair<Key, T>;

private:
  struct Node {
    value_type kv;
    Node *next = nullptr;
    std::size_t hash_value = 0;

    template <typename KArg, typename VArg>
    Node(KArg &&key, VArg &&value, std::size_t hv, Node *nxt)
        : kv(std::forward<KArg>(key), std::forward<VArg>(value)), next(nxt),
          hash_value(hv) {}
  };

  std::size_t size_ = 0;
  std::size_t bucket_count_ = 0;
  Node **buckets_ = nullptr;
  Hash hasher_;
  KeyEqual equal_;

  static constexpr std::size_t kInitialBucketCount = 16;

  void allocate_buckets(std::size_t count) {
    bucket_count_ = count;
    buckets_ = new Node *[bucket_count_];
    for (std::size_t i = 0; i < bucket_count_; ++i)
      buckets_[i] = nullptr;
  }

  void destroy_all_nodes() {
    if (buckets_ == nullptr)
      return;
    for (std::size_t i = 0; i < bucket_count_; ++i) {
      Node *cur = buckets_[i];
      while (cur != nullptr) {
        Node *nxt = cur->next;
        delete cur;
        cur = nxt;
      }
      buckets_[i] = nullptr;
    }
    size_ = 0;
  }

  void grow_for_ins() {
    if (bucket_count_ == 0) {
      allocate_buckets(kInitialBucketCount);
      return;
    }
    if ((size_ + 1) * 4 > bucket_count_ * 3) {
      rehash(bucket_count_ * 2);
    }
  }

  void rehash(std::size_t new_bucket_count) {
    if (new_bucket_count < kInitialBucketCount) {
      new_bucket_count = kInitialBucketCount;
    }

    Node **new_buckets = new Node *[new_bucket_count];
    for (std::size_t i = 0; i < new_bucket_count; ++i)
      new_buckets[i] = nullptr;

    for (std::size_t i = 0; i < bucket_count_; ++i) {
      Node *cur = buckets_[i];
      while (cur != nullptr) {
        Node *nxt = cur->next;
        std::size_t idx = cur->hash_value % new_bucket_count;
        cur->next = new_buckets[idx];
        new_buckets[idx] = cur;
        cur = nxt;
      }
      buckets_[i] = nullptr;
    }

    delete[] buckets_;
    buckets_ = new_buckets;
    bucket_count_ = new_bucket_count;
  }

public:
  class iterator {
    friend class hashmap;

  private:
    hashmap *map_ = nullptr;
    std::size_t bucket_idx_ = 0;
    Node *node_ = nullptr;

    iterator(hashmap *map, std::size_t bucket_idx, Node *node)
        : map_(map), bucket_idx_(bucket_idx), node_(node) {}

  public:
    iterator() = default;

    value_type &operator*() const { return node_->kv; }
    value_type *operator->() const { return &(node_->kv); }

    iterator &operator++() {
      if (node_ == nullptr || map_ == nullptr)
        return *this;
      if (node_->next != nullptr) {
        node_ = node_->next;
        return *this;
      }
      ++bucket_idx_;
      while (bucket_idx_ < map_->bucket_count_ &&
             map_->buckets_[bucket_idx_] == nullptr) {
        ++bucket_idx_;
      }
      if (bucket_idx_ >= map_->bucket_count_) {
        node_ = nullptr;
      } else {
        node_ = map_->buckets_[bucket_idx_];
      }
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const iterator &rhs) const {
      return map_ == rhs.map_ && node_ == rhs.node_;
    }
    bool operator!=(const iterator &rhs) const { return !(*this == rhs); }
  };

  class const_iterator {
    friend class hashmap;

  private:
    const hashmap *map_ = nullptr;
    std::size_t bucket_idx_ = 0;
    const Node *node_ = nullptr;

    const_iterator(const hashmap *map, std::size_t bucket_idx, const Node *node)
        : map_(map), bucket_idx_(bucket_idx), node_(node) {}

  public:
    const_iterator() = default;
    const_iterator(const iterator &it)
        : map_(it.map_), bucket_idx_(it.bucket_idx_), node_(it.node_) {}

    const value_type &operator*() const { return node_->kv; }
    const value_type *operator->() const { return &(node_->kv); }

    const_iterator &operator++() {
      if (node_ == nullptr || map_ == nullptr)
        return *this;
      if (node_->next != nullptr) {
        node_ = node_->next;
        return *this;
      }
      ++bucket_idx_;
      while (bucket_idx_ < map_->bucket_count_ &&
             map_->buckets_[bucket_idx_] == nullptr) {
        ++bucket_idx_;
      }
      if (bucket_idx_ >= map_->bucket_count_) {
        node_ = nullptr;
      } else {
        node_ = map_->buckets_[bucket_idx_];
      }
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const const_iterator &rhs) const {
      return map_ == rhs.map_ && node_ == rhs.node_;
    }
    bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }
  };

  hashmap() { allocate_buckets(kInitialBucketCount); }

  hashmap(const hashmap &other) {
    allocate_buckets(other.bucket_count_ == 0 ? kInitialBucketCount
                                              : other.bucket_count_);
    for (const_iterator it = other.begin(); it != other.end(); ++it) {
      emplace(it->first, it->second);
    }
  }

  hashmap &operator=(const hashmap &other) {
    if (this == &other)
      return *this;
    clear();
    delete[] buckets_;
    buckets_ = nullptr;
    allocate_buckets(other.bucket_count_ == 0 ? kInitialBucketCount
                                              : other.bucket_count_);
    for (const_iterator it = other.begin(); it != other.end(); ++it) {
      emplace(it->first, it->second);
    }
    return *this;
  }

  hashmap(hashmap &&other) noexcept
      : size_(other.size_), bucket_count_(other.bucket_count_),
        buckets_(other.buckets_), hasher_(other.hasher_), equal_(other.equal_) {
    other.size_ = 0;
    other.bucket_count_ = 0;
    other.buckets_ = nullptr;
  }

  hashmap &operator=(hashmap &&other) noexcept {
    if (this == &other)
      return *this;
    clear();
    delete[] buckets_;
    size_ = other.size_;
    bucket_count_ = other.bucket_count_;
    buckets_ = other.buckets_;
    hasher_ = other.hasher_;
    equal_ = other.equal_;

    other.size_ = 0;
    other.bucket_count_ = 0;
    other.buckets_ = nullptr;
    return *this;
  }

  ~hashmap() {
    clear();
    delete[] buckets_;
    buckets_ = nullptr;
    bucket_count_ = 0;
  }

  bool empty() const { return size_ == 0; }
  std::size_t size() const { return size_; }

  void clear() { destroy_all_nodes(); }

  iterator begin() {
    if (size_ == 0)
      return end();
    for (std::size_t i = 0; i < bucket_count_; ++i) {
      if (buckets_[i] != nullptr)
        return iterator(this, i, buckets_[i]);
    }
    return end();
  }

  iterator end() { return iterator(this, bucket_count_, nullptr); }

  const_iterator begin() const {
    if (size_ == 0)
      return end();
    for (std::size_t i = 0; i < bucket_count_; ++i) {
      if (buckets_[i] != nullptr)
        return const_iterator(this, i, buckets_[i]);
    }
    return end();
  }

  const_iterator end() const {
    return const_iterator(this, bucket_count_, nullptr);
  }

  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  iterator find(const Key &key) {
    if (bucket_count_ == 0)
      return end();
    std::size_t hv = hasher_(key);
    std::size_t idx = hv % bucket_count_;
    Node *cur = buckets_[idx];
    while (cur != nullptr) {
      if (cur->hash_value == hv && equal_(cur->kv.first, key)) {
        return iterator(this, idx, cur);
      }
      cur = cur->next;
    }
    return end();
  }

  const_iterator find(const Key &key) const {
    if (bucket_count_ == 0)
      return end();
    std::size_t hv = hasher_(key);
    std::size_t idx = hv % bucket_count_;
    const Node *cur = buckets_[idx];
    while (cur != nullptr) {
      if (cur->hash_value == hv && equal_(cur->kv.first, key)) {
        return const_iterator(this, idx, cur);
      }
      cur = cur->next;
    }
    return end();
  }

  template <typename KArg, typename VArg>
  pair<iterator, bool> emplace(KArg &&key_arg, VArg &&value_arg) {
    grow_for_ins();

    Key key(std::forward<KArg>(key_arg));
    std::size_t hv = hasher_(key);
    std::size_t idx = hv % bucket_count_;
    Node *cur = buckets_[idx];
    while (cur != nullptr) {
      if (cur->hash_value == hv && equal_(cur->kv.first, key)) {
        return pair<iterator, bool>(iterator(this, idx, cur), false);
      }
      cur = cur->next;
    }

    Node *inserted = new Node(std::move(key), std::forward<VArg>(value_arg), hv,
                              buckets_[idx]);
    buckets_[idx] = inserted;
    ++size_;
    return pair<iterator, bool>(iterator(this, idx, inserted), true);
  }

  pair<iterator, bool> insert(const value_type &value) {
    return emplace(value.first, value.second);
  }

  T &operator[](const Key &key) {
    iterator it = find(key);
    if (it != end())
      return it->second;
    return emplace(key, T()).first->second;
  }

  std::size_t erase(const Key &key) {
    if (bucket_count_ == 0)
      return 0;
    std::size_t hv = hasher_(key);
    std::size_t idx = hv % bucket_count_;
    Node *cur = buckets_[idx];
    Node *pre = nullptr;
    while (cur != nullptr) {
      if (cur->hash_value == hv && equal_(cur->kv.first, key)) {
        if (pre == nullptr)
          buckets_[idx] = cur->next;
        else
          pre->next = cur->next;
        delete cur;
        --size_;
        return 1;
      }
      pre = cur;
      cur = cur->next;
    }
    return 0;
  }

  void erase(iterator pos) {
    if (pos.map_ != this || pos.node_ == nullptr || bucket_count_ == 0)
      return;
    Node *cur = buckets_[pos.bucket_idx_];
    Node *pre = nullptr;
    while (cur != nullptr) {
      if (cur == pos.node_) {
        if (pre == nullptr)
          buckets_[pos.bucket_idx_] = cur->next;
        else
          pre->next = cur->next;
        delete cur;
        --size_;
        return;
      }
      pre = cur;
      cur = cur->next;
    }
  }
};

} // namespace sjtu

#endif