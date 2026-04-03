#ifndef SJTU_VECTOR_HPP
#define SJTU_VECTOR_HPP
#pragma GCC("Ofast")
#include <cstddef>
#include <cstring>
#include <iostream>
#include <iterator>
#include <utility>

#include "../memoryriver/memoryriver.hpp"
#include "exceptions.hpp"

namespace sjtu {

template <typename T> class vector {
public:
  class const_iterator;
  class iterator {

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T *;
    using reference = T &;
    using iterator_category = std::random_access_iterator_tag;
    friend class const_iterator;
    friend class vector;

  private:
    T *ptr_;
    T *begin_;

  public:
    iterator() = delete;
    iterator(T *begin_, T *ptr_) : ptr_(ptr_), begin_(begin_) {}
    iterator(const_iterator &rhs) : ptr_(rhs.ptr_), begin_(rhs.begin_) {}
    iterator operator+(const int &n) const {

      return iterator(begin_, ptr_ + n);
    }
    iterator operator-(const int &n) const {
      return iterator(begin_, ptr_ - n);
    }

    difference_type operator-(const iterator &rhs) const {
      if (rhs.begin_ != begin_)
        throw invalid_iterator();
      return ptr_ - rhs.ptr_;
    }
    iterator &operator+=(const int &n) {
      ptr_ = ptr_ + n;
      return *this;
    }
    iterator &operator-=(const int &n) {
      ptr_ = ptr_ - n;
      return *this;
    }

    iterator operator++(int) {
      T *tmp = ptr_;
      ++ptr_;
      return iterator(begin_, tmp);
    }

    iterator &operator++() {
      ++ptr_;
      return *this;
    }

    iterator operator--(int) {
      T *tmp = ptr_;
      --ptr_;
      return iterator(begin_, tmp);
    }

    iterator &operator--() {
      --ptr_;
      return *this;
    }

    T &operator*() const { return *ptr_; }
    T *operator->() const { return ptr_; }

    bool operator==(const iterator &rhs) const {
      return (begin_ == rhs.begin_) && (ptr_ == rhs.ptr_);
    }
    bool operator==(const const_iterator &rhs) const {
      return (begin_ == rhs.begin_) && (ptr_ == rhs.ptr_);
    }

    bool operator!=(const iterator &rhs) const { return !(*this == rhs); }
    bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }
    bool operator<(const iterator &rhs) const {
      if (begin_ != rhs.begin_)
        throw invalid_iterator();
      return ptr_ < rhs.ptr_;
    }
    bool operator<(const const_iterator &rhs) const {
      if (begin_ != rhs.begin_)
        throw invalid_iterator();
      return ptr_ < rhs.ptr_;
    }
    bool operator>(const iterator &rhs) const { return rhs < *this; }
    bool operator>(const const_iterator &rhs) const { return rhs < *this; }
    bool operator<=(const iterator &rhs) const { return !(*this > rhs); }
    bool operator<=(const const_iterator &rhs) const { return !(*this > rhs); }
    bool operator>=(const iterator &rhs) const { return !(*this < rhs); }
    bool operator>=(const const_iterator &rhs) const { return !(*this < rhs); }
  };

  class const_iterator {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T *;
    using reference = T &;
    using iterator_category = std::random_access_iterator_tag;
    friend class iterator;
    friend class vector;

  private:
    const T *begin_;
    const T *ptr_;

  public:
    const_iterator() = delete;
    const_iterator(const T *begin_, const T *ptr_)
        : ptr_(ptr_), begin_(begin_) {}
    const_iterator(const iterator &rhs) : ptr_(rhs.ptr_), begin_(rhs.begin_) {}
    const_iterator operator+(const int &n) const {

      return const_iterator(begin_, ptr_ + n);
    }
    const_iterator operator-(const int &n) const {
      return const_iterator(begin_, ptr_ - n);
    }

    difference_type operator-(const iterator &rhs) const {
      if (rhs.begin_ != begin_)
        throw invalid_iterator();
      return ptr_ - rhs.ptr_;
    }
    difference_type operator-(const const_iterator &rhs) const {
      if (rhs.begin_ != begin_)
        throw invalid_iterator();
      return ptr_ - rhs.ptr_;
    }

    const_iterator &operator+=(const int &n) {
      ptr_ = ptr_ + n;
      return *this;
    }
    const_iterator &operator-=(const int &n) {
      ptr_ = ptr_ - n;
      return *this;
    }

    const_iterator operator++(int) {
      const T *tmp = ptr_;
      ++ptr_;
      return const_iterator(begin_, tmp);
    }

    const_iterator &operator++() {
      ++ptr_;
      return *this;
    }

    const_iterator operator--(int) {
      const T *tmp = ptr_;
      --ptr_;
      return const_iterator(begin_, tmp);
    }

    const_iterator &operator--() {
      --ptr_;
      return *this;
    }

    const T &operator*() const { return *ptr_; }
    const T *operator->() const { return ptr_; }

    bool operator==(const iterator &rhs) const {
      return (begin_ == rhs.begin_) && (ptr_ == rhs.ptr_);
    }
    bool operator==(const const_iterator &rhs) const {
      return (begin_ == rhs.begin_) && (ptr_ == rhs.ptr_);
    }

    bool operator!=(const iterator &rhs) const { return !(*this == rhs); }
    bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }
    bool operator<(const iterator &rhs) const {
      if (begin_ != rhs.begin_)
        throw invalid_iterator();
      return ptr_ < rhs.ptr_;
    }
    bool operator<(const const_iterator &rhs) const {
      if (begin_ != rhs.begin_)
        throw invalid_iterator();
      return ptr_ < rhs.ptr_;
    }
    bool operator>(const iterator &rhs) const { return rhs < *this; }
    bool operator>(const const_iterator &rhs) const { return rhs < *this; }
    bool operator<=(const iterator &rhs) const { return !(*this > rhs); }
    bool operator<=(const const_iterator &rhs) const { return !(*this > rhs); }
    bool operator>=(const iterator &rhs) const { return !(*this < rhs); }
    bool operator>=(const const_iterator &rhs) const { return !(*this < rhs); }
  };

  vector() : data(nullptr), capacity(0), size_(0) {}
  vector(const vector &other) : capacity(other.capacity), size_(other.size_) {
    data = static_cast<T *>(operator new(capacity * sizeof(T)));
    for (size_t i = 0; i < size_; ++i)
      new (&data[i]) T(other.data[i]);
  }

  ~vector() {
    for (size_t i = 0; i < size_; ++i)
      data[i].~T();
    operator delete(data);
  }

  vector &operator=(const vector &other) {
    if (this == &other)
      return *this;
    if (capacity >= other.size_) {
      size_t min_size = (size_ < other.size_) ? size_ : other.size_;
      for (size_t i = 0; i < min_size; ++i)
        data[i] = other.data[i];
      for (size_t i = size_; i < other.size_; ++i)
        new (&data[i]) T(other.data[i]);
      for (size_t i = other.size_; i < size_; ++i)
        data[i].~T();

      size_ = other.size_;
      return *this;
      ·
    }
    for (size_t i = 0; i < size_; ++i)
      data[i].~T();
    operator delete(data);
    capacity = other.capacity;
    size_ = other.size_;
    data = static_cast<T *>(operator new(capacity * sizeof(T)));
    if constexpr (std::is_trivially_copyable<T>::value) {
      std::memcpy(data, other.data, size_ * sizeof(T));
    } else {
      for (size_t i = 0; i < size_; ++i)
        new (&data[i]) T(other.data[i]);
    }

    return *this;
  }
  vector(vector &&other) noexcept
      : data(other.data), size_(other.size_), capacity(other.capacity) {
    other.data = nullptr;
    other.size_ = other.capacity = 0;
  }
  vector &operator=(vector &&other) noexcept {
    if (this != &other) {
      for (size_t i = 0; i < size_; ++i)
        data[i].~T();
      operator delete(data);

      data = other.data;
      size_ = other.size_;
      capacity = other.capacity;

      other.data = nullptr;
      other.size_ = 0;
      other.capacity = 0;
    }
    return *this;
  }

  T &at(const size_t &pos) {
    if (pos >= size_) {
      std::cerr << "at pos " << pos << " size " << size_ << "\n";
      throw idx_oob();
    }
    return data[pos];
  }
  const T &at(const size_t &pos) const {
    if (pos >= size_) {
      std::cerr << "at pos " << pos << " size " << size_ << "\n";
      throw idx_oob();
    }
    return data[pos];
  }

  T &operator[](const size_t &pos) {
    if (pos >= size_) {
      std::cerr << "operator[] pos " << pos << " size " << size_ << "\n";
      throw idx_oob();
    }
    return data[pos];
  }
  const T &operator[](const size_t &pos) const {
    if (pos >= size_) {
      std::cerr << "operator[] pos " << pos << " size " << size_ << "\n";
      throw idx_oob();
    }
    return data[pos];
  }

  void serialize(std::ostream &os) const {
    size_t sz = size_;
    os.write(reinterpret_cast<const char *>(&sz), sizeof(sz));
    for (size_t i = 0; i < sz; ++i)
      sjtu::Serializer<T>::write(os, data[i]);
  }
  void deserialize(std::istream &is) {
    size_t sz;
    is.read(reinterpret_cast<char *>(&sz), sizeof(sz));

    if (sz > 10000000) {
      throw std::bad_alloc();
    }

    clear();

    for (size_t i = 0; i < sz; ++i) {
      T tmp;
      sjtu::Serializer<T>::read(is, tmp);
      push_back(tmp);
    }
  }

  const T &front() const {
    if (size_ == 0)
      throw container_is_empty();
    return data[0];
  }

  const T &back() const {
    if (size_ == 0)
      throw container_is_empty();
    return data[size_ - 1];
  }

  iterator begin() { return iterator(data, data); }
  const_iterator begin() const { return const_iterator(data, data); }
  const_iterator cbegin() const { return const_iterator(data, data); }

  iterator end() { return iterator(data, data + size_); }
  const_iterator end() const { return const_iterator(data, data + size_); }
  const_iterator cend() const { return const_iterator(data, data + size_); }

  bool empty() const { return size_ == 0; }

  size_t size() const { return size_; }
  void reserve(size_t new_capacity) {
    if (new_capacity <= capacity)
      return;
    T *newdata = static_cast<T *>(operator new(new_capacity * sizeof(T)));
    if constexpr (std::is_trivially_copyable<T>::value) {
      if (size_ > 0) {
        std::memcpy(newdata, data, size_ * sizeof(T));
      }
    } else {
      for (size_t i = 0; i < size_; ++i) {
        new (&newdata[i]) T(std::move(data[i]));
        data[i].~T();
      }
    }
    operator delete(data);
    data = newdata;
    capacity = new_capacity;
  }
  void resize(size_t new_size) {
    if (new_size < size_) {
      for (size_t i = new_size; i < size_; ++i)
        data[i].~T();
      size_ = new_size;
      return;
    }
    if (new_size == size_)
      return;
    if (new_size > capacity) {
      size_t target = (capacity == 0 ? 1 : capacity);
      while (target < new_size)
        target <<= 1;
      reserve(target);
    }
    for (size_t i = size_; i < new_size; ++i)
      new (&data[i]) T();
    size_ = new_size;
  }
  void resize(size_t new_size, const T &value) {
    if (new_size < size_) {
      for (size_t i = new_size; i < size_; ++i)
        data[i].~T();
      size_ = new_size;
      return;
    }
    if (new_size == size_)
      return;
    if (new_size > capacity) {
      size_t target = (capacity == 0 ? 1 : capacity);
      while (target < new_size)
        target <<= 1;
      reserve(target);
    }
    for (size_t i = size_; i < new_size; ++i)
      new (&data[i]) T(value);
    size_ = new_size;
  }
  void assign(size_t count, const T &value) {
    clear();
    if (count == 0)
      return;
    reserve(count);
    for (size_t i = 0; i < count; ++i) {
      new (&data[i]) T(value);
    }
    size_ = count;
  }

  void clear() {
    for (size_t i = 0; i < size_; ++i)
      data[i].~T();
    operator delete(data);
    size_ = capacity = 0;
    data = nullptr;
  }

  iterator insert(iterator pos, const T &value) {
    return insert(pos.ptr_ - pos.begin_, value);
  }

  iterator insert(const size_t &ind, const T &value) {
    if (ind > size_) {
      std::cerr << "insert ind " << ind << " size " << size_ << "\n";
      throw idx_oob();
    }
    if (size_ == capacity)
      expand();
    if (size_ > ind) {
      new (&data[size_]) T(std::move(data[size_ - 1]));
      for (size_t i = size_ - 1; i > ind; i--) {
        data[i] = std::move(data[i - 1]);
      }
      data[ind] = std::move(value);
    } else
      new (&data[size_]) T(std::move(value));
    ++size_;
    return iterator(data, data + ind);
  }
  iterator insert(iterator pos, T &&value) {
    return insert(pos.ptr_ - pos.begin_, std::move(value));
  }
  iterator insert(const size_t &ind, T &&value) {
    if (ind > size_) {
      std::cerr << "insert ind " << ind << " size " << size_ << "\n";
      throw idx_oob();
    }
    if (size_ == capacity)
      expand();

    if (size_ > ind) {
      new (&data[size_]) T(std::move(data[size_ - 1]));
      for (size_t i = size_ - 1; i > ind; --i) {
        data[i] = std::move(data[i - 1]);
      }
      data[ind] = std::move(value);
    } else {
      new (&data[size_]) T(std::move(value));
    }
    ++size_;
    return iterator(data, data + ind);
  }

  iterator erase(iterator pos) { return erase(pos.ptr_ - pos.begin_); }

  iterator erase(const size_t &ind) {
    if (ind >= size_) {
      std::cerr << "erase ind " << ind << " size " << size_ << "\n";
      throw idx_oob();
    }
    for (size_t i = ind; i < size_ - 1; ++i) {
      data[i] = std::move(data[i + 1]);
    }
    data[size_ - 1].~T();
    --size_;
    return iterator(data, data + ind);
  }

  void push_back(const T &value) {
    if (size_ == capacity)
      expand();
    new (&data[size_++]) T(value);
  }
  void push_back(T &&value) {
    if (size_ == capacity)
      expand();
    new (&data[size_++]) T(std::move(value));
  }

  void pop_back() {
    if (size_ == 0)
      throw container_is_empty();
    data[--size_].~T();
  }

#include <cstring>
#include <type_traits>

  void expand() {
    if (capacity == 0)
      capacity = 1;
    else
      capacity *= 2;

    T *newdata = static_cast<T *>(operator new(capacity * sizeof(T)));

    if constexpr (std::is_trivially_copyable<T>::value) {
      if (size_ > 0) {
        std::memcpy(newdata, data, size_ * sizeof(T));
      }
    } else {
      for (size_t i = 0; i < size_; ++i) {
        new (&newdata[i]) T(std::move(data[i]));
        data[i].~T();
      }
    }

    operator delete(data);
    data = newdata;
  }
  void sort(bool (*compare)(T &, T &));

private:
  size_t size_, capacity;
  T *data;
};

template <typename T>
void quick_sort(T *arr, int low, int high, bool (*compare)(T &, T &)) {
  if (low < high) {

    int pi = partition(arr, low, high, compare);

    quick_sort(arr, low, pi - 1, compare);
    quick_sort(arr, pi + 1, high, compare);
  }
}

template <typename T>
int partition(T *arr, int low, int high, bool (*compare)(T &, T &)) {
  T pivot = arr[high];
  int i = (low - 1);

  for (int j = low; j <= high - 1; j++) {

    if (compare(arr[j], pivot)) {
      i++;
      std::swap(arr[i], arr[j]);
    }
  }
  std::swap(arr[i + 1], arr[high]);
  return (i + 1);
}

template <typename T> void vector<T>::sort(bool (*compare)(T &, T &)) {
  if (size_ > 1) {
    quick_sort(data, 0, size_ - 1, compare);
  }
}

} // namespace sjtu

#endif
