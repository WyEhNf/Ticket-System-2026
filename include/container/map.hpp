

#ifndef SJTU_MAP_HPP
#define SJTU_MAP_HPP

#include "../memoryriver/memoryriver.hpp"
#include "exceptions.hpp"
#include "utility.hpp"
#include <cstddef>
#include <functional>
#include <iostream>

namespace sjtu {

enum Color { RED, BLACK };

template <typename K, typename V> struct Node {
  typedef pair<const K, V> value_type;
  value_type data;
  Color color;
  Node *l_son;
  Node *r_son;
  Node *par;
  Node(K key, V value, Color color) : data(key, value), color(color) {}
};

template <typename K, typename V, class Compare = std::less<K>> class RBtree {
  Compare comp;
  using NodeType = Node<K, V>;

private:
  NodeType *root;
  NodeType *NIL;

  NodeType *copyTree(NodeType *cur, NodeType *otherNIL, NodeType *parent) {
    if (cur == otherNIL)
      return NIL;
    NodeType *newNode =
        new NodeType(cur->data.first, cur->data.second, cur->color);
    newNode->par = parent;
    newNode->l_son = copyTree(cur->l_son, otherNIL, newNode);
    newNode->r_son = copyTree(cur->r_son, otherNIL, newNode);
    return newNode;
  }

  void leftRotate(NodeType *x) {
    NodeType *y = x->r_son;
    x->r_son = y->l_son;
    if (y->l_son != NIL)
      y->l_son->par = x;
    y->par = x->par;
    if (x->par == NIL)
      root = y;
    else if (x->par->l_son == x)
      x->par->l_son = y;
    else
      x->par->r_son = y;
    x->par = y;
    y->l_son = x;
    NIL->par = NIL;
  }
  void rightRotate(NodeType *x) {
    NodeType *y = x->l_son;
    x->l_son = y->r_son;
    if (y->r_son != NIL)
      y->r_son->par = x;
    y->par = x->par;
    if (x->par == NIL)
      root = y;
    else if (x->par->l_son == x)
      x->par->l_son = y;
    else
      x->par->r_son = y;
    x->par = y;
    y->r_son = x;
    NIL->par = NIL;
  }
  void insertFixup(NodeType *z) {
    while (z != root && z->par->color == RED) {

      if (z->par == z->par->par->l_son) {
        NodeType *y = z->par->par->r_son;
        if (y->color == RED) {
          z->par->color = BLACK;
          y->color = BLACK;
          z->par->par->color = RED;
          z = z->par->par;
        } else {
          if (z == z->par->r_son) {
            z = z->par;
            leftRotate(z);
          }

          z->par->color = BLACK;
          z->par->par->color = RED;
          rightRotate(z->par->par);
        }
      } else {

        NodeType *y = z->par->par->l_son;
        if (y->color == RED) {
          z->par->color = BLACK;
          y->color = BLACK;
          z->par->par->color = RED;
          z = z->par->par;
        } else {
          if (z == z->par->l_son) {
            z = z->par;
            rightRotate(z);
          }
          z->par->color = BLACK;
          z->par->par->color = RED;
          leftRotate(z->par->par);
        }
      }
      NIL->par = NIL;
      NIL->color = BLACK;
    }
    root->color = BLACK;
  }
  void deleteFixup(NodeType *x) {
    while (x != root && x->par->color == BLACK) {
      if (x == NIL)
        break;
      if (x == x->par->l_son) {
        NodeType *y = x->par->r_son;
        if (y->color == RED) {
          y->color = BLACK;
          x->par->color = RED;
          leftRotate(x->par);
          y = x->par->r_son;
        }
        NIL->color = BLACK;
        if (y->l_son->color == BLACK && y->r_son->color == BLACK) {
          y->color = RED;
          x = x->par;
        } else {
          if (y->l_son->color == RED && y->r_son->color == BLACK) {
            y->l_son->color = BLACK;
            y->color = RED;
            rightRotate(y);
            y = y->par;
          }
          NIL->color = BLACK;
          y->color = x->par->color;
          x->par->color = BLACK;
          y->r_son->color = BLACK;
          leftRotate(x->par);
          break;
        }
      } else {
        NodeType *y = x->par->l_son;
        if (y->color == RED) {
          y->color = BLACK;
          x->par->color = RED;
          rightRotate(x->par);
          y = x->par->l_son;
        }
        NIL->color = BLACK;
        if (y->r_son->color == BLACK && y->l_son->color == BLACK) {
          y->color = RED;
          x = x->par;
        } else {
          if (y->r_son->color == RED && y->l_son->color == BLACK) {
            y->r_son->color = BLACK;
            y->color = RED;
            leftRotate(y);
            y = y->par;
          }
          NIL->color = BLACK;
          y->color = x->par->color;
          x->par->color = BLACK;
          y->l_son->color = BLACK;
          rightRotate(x->par);
          break;
        }
      }
      NIL->par = NIL;
      NIL->color = BLACK;
    }
    root->color = BLACK;
  }
  void transplant(NodeType *u, NodeType *v) {
    if (u->par == NIL)
      root = v;
    else if (u->par->l_son == u)
      u->par->l_son = v;
    else
      u->par->r_son = v;
    v->par = u->par;
    NIL->par = NIL;
  }

public:
  RBtree() {
    NIL = static_cast<NodeType *>(operator new(sizeof(NodeType)));
    NIL->l_son = NIL;
    NIL->r_son = NIL;
    NIL->par = NIL;
    NIL->color = BLACK;
    root = NIL;
  }
  ~RBtree() {
    clear(root);
    operator delete(NIL);
  }
  RBtree(const RBtree &other) {
    NIL = static_cast<NodeType *>(operator new(sizeof(NodeType)));
    NIL->l_son = NIL->r_son = NIL->par = NIL;
    NIL->color = BLACK;
    root = copyTree(other.root, other.NIL, NIL);
  }
  RBtree &operator=(const RBtree &other) {
    if (this == &other)
      return *this;
    RBtree temp(other);
    this->swap(temp);
    return *this;
  }

  void swap(RBtree &other) {
    std::swap(root, other.root);
    std::swap(NIL, other.NIL);
  }

  void clear(NodeType *cur) {
    if (cur == NIL)
      return;
    clear(cur->l_son);
    clear(cur->r_son);
    delete cur;
  }
  void clear() {
    clear(root);
    root = NIL;
  }
  NodeType *insert(K key, V value) {
    NodeType *y = nullptr;
    NodeType *x = root;
    while (x != NIL) {
      y = x;
      if (comp(key, x->data.first))
        x = x->l_son;
      else if (comp(x->data.first, key))
        x = x->r_son;
      else
        return x;
    }
    NodeType *z = new NodeType(key, value, RED);
    if (y == nullptr) {
      z->par = NIL;
      root = z;
      root->color = BLACK;
    } else {
      z->par = y;
      if (comp(z->data.first, y->data.first))
        y->l_son = z;
      else
        y->r_son = z;
    }
    z->l_son = z->r_son = NIL;
    insertFixup(z);
    return z;
  }
  void remove(K key) {
    NodeType *z = find(key);
    if (z == NIL)
      return;
    NodeType *y = z;
    NodeType *x;
    Color old_col = y->color;
    if (z->l_son == NIL) {
      x = z->r_son;
      transplant(z, x);
    } else if (z->r_son == NIL) {
      x = z->l_son;
      transplant(z, x);
    } else {
      y = z->r_son;
      while (y->l_son != NIL)
        y = y->l_son;
      old_col = y->color;
      x = y->r_son;
      if (y->par == z && x->par != NIL)
        x->par = y;
      else {
        transplant(y, x);
        y->r_son = z->r_son;
        if (y->r_son != NIL)
          y->r_son->par = y;
      }
      transplant(z, y);
      y->l_son = z->l_son;
      if (y->l_son != NIL)
        y->l_son->par = y;
      y->color = z->color;
    }
    if (old_col == BLACK)
      deleteFixup(x);
    delete z;
    NIL->par = NIL;
  }
  NodeType *find(K key) const {
    NodeType *x = root;
    while (x != NIL && (comp(key, x->data.first) || comp(x->data.first, key))) {
      if (comp(key, x->data.first))
        x = x->l_son;
      else
        x = x->r_son;
    }
    return x;
  }
  NodeType *getNIL() const { return NIL; }
  NodeType *getRoot() const { return root; }
};

template <class Key, class T, class Compare = std::less<Key>> class map {
private:
  RBtree<Key, T, Compare> tree;
  std::size_t size_;

public:
  typedef pair<const Key, T> value_type;

  class const_iterator;
  class iterator {
    using NodeType = Node<Key, T>;

  private:
    RBtree<Key, T, Compare> *tr_;
    NodeType *ptr_;
    friend class const_iterator;
    friend class map;

  public:
    iterator() : tr_(nullptr), ptr_(nullptr) {}
    iterator(RBtree<Key, T, Compare> *tr_, NodeType *ptr_)
        : tr_(tr_), ptr_(ptr_) {}
    iterator(const iterator &other) : tr_(other.tr_), ptr_(other.ptr_) {}

    NodeType *get_first() {
      NodeType *res = tr_->getRoot();
      if (res == nullptr)
        return tr_->getNIL();
      while (res->l_son != tr_->getNIL())
        res = res->l_son;
      return res;
    }
    NodeType *get_last() {
      NodeType *res = tr_->getRoot();
      if (res == nullptr)
        return tr_->getNIL();
      while (res->r_son != tr_->getNIL())
        res = res->r_son;
      return res;
    }

    iterator operator++(int) {
      if (ptr_ == tr_->getNIL())
        throw invalid_iterator();
      NodeType *tmp = ptr_;
      if (ptr_->r_son != tr_->getNIL()) {

        ptr_ = ptr_->r_son;
        while (ptr_->l_son != tr_->getNIL())
          ptr_ = ptr_->l_son;
      } else {
        while (ptr_->par != tr_->getNIL() && ptr_->par->l_son != ptr_)
          ptr_ = ptr_->par;
        ptr_ = ptr_->par;
      }
      return iterator(tr_, tmp);
    }

    iterator &operator++() {
      if (ptr_ == tr_->getNIL())
        throw invalid_iterator();
      if (ptr_->r_son != tr_->getNIL()) {

        ptr_ = ptr_->r_son;
        while (ptr_->l_son != tr_->getNIL())
          ptr_ = ptr_->l_son;
      } else {
        while (ptr_->par != tr_->getNIL() && ptr_->par->l_son != ptr_)
          ptr_ = ptr_->par;
        ptr_ = ptr_->par;
      }
      return *this;
    }

    iterator operator--(int) {
      if (ptr_ == get_first())
        throw invalid_iterator();
      NodeType *tmp = ptr_;
      if (ptr_ == tr_->getNIL()) {
        ptr_ = get_last();
      } else if (ptr_->l_son != tr_->getNIL()) {
        ptr_ = ptr_->l_son;
        while (ptr_->r_son != tr_->getNIL())
          ptr_ = ptr_->r_son;
      } else {
        while (ptr_->par != tr_->getNIL() && ptr_->par->r_son != ptr_)
          ptr_ = ptr_->par;
        ptr_ = ptr_->par;
      }
      return iterator(tr_, tmp);
    }

    iterator &operator--() {
      if (ptr_ == get_first())
        throw invalid_iterator();
      if (ptr_ == tr_->getNIL()) {
        ptr_ = get_last();
      } else if (ptr_->l_son != tr_->getNIL()) {
        ptr_ = ptr_->l_son;
        while (ptr_->r_son != tr_->getNIL())
          ptr_ = ptr_->r_son;
      } else {
        while (ptr_->par != tr_->getNIL() && ptr_->par->r_son != ptr_)
          ptr_ = ptr_->par;
        ptr_ = ptr_->par;
      }
      return *this;
    }

    value_type &operator*() const { return (*ptr_).data; }

    bool operator==(const iterator &rhs) const {
      return (tr_->getRoot() == rhs.tr_->getRoot()) && (ptr_ == rhs.ptr_);
    }

    bool operator==(const const_iterator &rhs) const {
      return (tr_->getRoot() == rhs.tr_->getRoot()) && (ptr_ == rhs.ptr_);
    }

    bool operator!=(const iterator &rhs) const { return !(*this == rhs); }

    bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }

    value_type *operator->() const noexcept { return &ptr_->data; }
  };
  class const_iterator {

    using NodeType = Node<Key, T>;

  private:
    const RBtree<Key, T, Compare> *tr_;
    const NodeType *ptr_;
    const NodeType *NIL;
    friend class iterator;
    friend class map;

  public:
    const_iterator() : tr_(nullptr), ptr_(nullptr) {}
    const_iterator(const RBtree<Key, T, Compare> *tr_, const NodeType *ptr_)
        : tr_(tr_), ptr_(ptr_) {}
    const_iterator(const iterator &other) : tr_(other.tr_), ptr_(other.ptr_) {}

    const NodeType *get_first() {
      const NodeType *res = tr_->getRoot();
      if (res == nullptr)
        return tr_->getNIL();
      while (res->l_son != tr_->getNIL())
        res = res->l_son;
      return res;
    }
    const NodeType *get_last() {
      const NodeType *res = tr_->getRoot();
      if (res == nullptr)
        return tr_->getNIL();
      while (res->r_son != tr_->getNIL())
        res = res->r_son;
      return res;
    }

    const_iterator operator++(int) {
      if (ptr_ == tr_->getNIL())
        throw invalid_iterator();
      const NodeType *tmp = ptr_;
      if (ptr_->r_son != tr_->getNIL()) {
        ptr_ = ptr_->r_son;
        while (ptr_->l_son != tr_->getNIL())
          ptr_ = ptr_->l_son;
      } else {
        while (ptr_->par != tr_->getNIL() && ptr_->par->l_son != ptr_)
          ptr_ = ptr_->par;
        ptr_ = ptr_->par;
      }
      return const_iterator(tr_, tmp);
    }

    const_iterator &operator++() {
      if (ptr_ == tr_->getNIL())
        throw invalid_iterator();
      if (ptr_->r_son != tr_->getNIL()) {
        ptr_ = ptr_->r_son;
        while (ptr_->l_son != tr_->getNIL())
          ptr_ = ptr_->l_son;
      } else {
        while (ptr_->par != tr_->getNIL() && ptr_->par->l_son != ptr_)
          ptr_ = ptr_->par;
        ptr_ = ptr_->par;
      }
      return *this;
    }

    const_iterator operator--(int) {
      if (ptr_ == get_first())
        throw invalid_iterator();
      const NodeType *tmp = ptr_;
      if (ptr_ == tr_->getNIL()) {
        ptr_ = get_last();
      } else if (ptr_->l_son != tr_->getNIL()) {
        ptr_ = ptr_->l_son;
        while (ptr_->r_son != tr_->getNIL())
          ptr_ = ptr_->r_son;
      } else {
        while (ptr_->par != tr_->getNIL() && ptr_->par->r_son != ptr_)
          ptr_ = ptr_->par;
        ptr_ = ptr_->par;
      }
      return const_iterator(tr_, tmp);
    }

    const_iterator &operator--() {
      if (ptr_ == get_first())
        throw invalid_iterator();
      if (ptr_ == tr_->getNIL()) {
        ptr_ = get_last();
      } else if (ptr_->l_son != tr_->getNIL()) {
        ptr_ = ptr_->l_son;
        while (ptr_->r_son != tr_->getNIL())
          ptr_ = ptr_->r_son;
      } else {
        while (ptr_->par != tr_->getNIL() && ptr_->par->r_son != ptr_)
          ptr_ = ptr_->par;
        ptr_ = ptr_->par;
      }
      return *this;
    }

    const value_type &operator*() const { return (*ptr_).data; }

    bool operator==(const iterator &rhs) const {
      return (tr_->getRoot() == rhs.tr_->getRoot()) && (ptr_ == rhs.ptr_);
    }

    bool operator==(const const_iterator &rhs) const {
      return (tr_->getRoot() == rhs.tr_->getRoot()) && (ptr_ == rhs.ptr_);
    }

    bool operator!=(const iterator &rhs) const { return !(*this == rhs); }

    bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }

    const value_type *operator->() const noexcept { return &ptr_->data; }
  };

  using NodeType = Node<Key, T>;
  map() : size_(0) {}

  map(const map &other) : size_(other.size_), tree(other.tree) {}

  map &operator=(const map &other) {
    if (this == &other)
      return *this;
    tree = other.tree;
    size_ = other.size_;
    return *this;
  }

  ~map() {}

  T &at(const Key &key) {
    NodeType *res = tree.find(key);
    if (res == tree.getNIL())
      throw idx_oob();
    return res->data.second;
  }

  const T &at(const Key &key) const {
    const NodeType *res = tree.find(key);
    if (res == tree.getNIL())
      throw idx_oob();
    return res->data.second;
  }

  T &operator[](const Key &key) {
    NodeType *res = tree.find(key);
    if (res == tree.getNIL()) {

      res = tree.insert(key, T()), ++size_;
    }
    return res->data.second;
  }

  const T &operator[](const Key &key) const {
    const NodeType *res = tree.find(key);
    if (res == tree.getNIL())
      throw idx_oob();
    return res->data.second;
  }

  iterator begin() {
    NodeType *cur = tree.getRoot();
    if (cur == tree.getNIL())
      return iterator(&tree, cur);
    while (cur->l_son != tree.getNIL())
      cur = cur->l_son;
    return iterator(&tree, cur);
  }

  const_iterator cbegin() const {
    const NodeType *cur = tree.getRoot();
    if (cur == tree.getNIL())
      return const_iterator(&tree, cur);
    while (cur->l_son != tree.getNIL())
      cur = cur->l_son;
    return const_iterator(&tree, cur);
  }

  iterator end() { return iterator(&tree, tree.getNIL()); }

  const_iterator cend() const { return const_iterator(&tree, tree.getNIL()); }

  bool empty() const { return tree.getRoot() == tree.getNIL(); }

  size_t size() const { return size_; }

  void serialize(std::ostream &os) const {
    size_t sz = size_;
    os.write(reinterpret_cast<const char *>(&sz), sizeof(sz));
    for (const_iterator it = cbegin(); it != cend(); ++it) {
      Serializer<value_type>::write(os, *it);
    }
  }
  void deserialize(std::istream &is) {
    size_t sz;
    is.read(reinterpret_cast<char *>(&sz), sizeof(sz));
    clear();
    for (size_t i = 0; i < sz; ++i) {
      value_type vt;
      Serializer<value_type>::read(is, vt);
      insert(vt);
    }
  }

  void clear() {
    tree.clear();
    size_ = 0;
  }

  pair<iterator, bool> insert(const value_type &value) {
    NodeType *tmp = tree.find(value.first);

    if (tmp == tree.getNIL()) {

      NodeType *res = tree.insert(value.first, value.second);
      ++size_;
      return pair(iterator(&tree, res), true);
    } else
      return pair(iterator(&tree, tmp), false);
  }

  void erase(iterator pos) {
    if (pos.ptr_ == tree.getNIL() || pos.tr_->getRoot() != tree.getRoot())
      throw invalid_iterator();
    tree.remove(pos.ptr_->data.first);
    --size_;
  }

  size_t count(const Key &key) const {
    if (tree.find(key) != tree.getNIL())
      return 1;
    else
      return 0;
  }

  iterator find(const Key &key) { return iterator(&tree, tree.find(key)); }

  const_iterator find(const Key &key) const {
    return const_iterator(&tree, tree.find(key));
  }
};

} // namespace sjtu

#endif
