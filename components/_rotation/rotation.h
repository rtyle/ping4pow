#pragma once

#include <list>
#include <iterator>

#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace _rotation {

template<typename T, typename Allocator = std::allocator<T>> class Rotation : public Component {
 public:
  class Iterator {
   private:
    typename std::list<T, Allocator> *list_;
    typename std::list<T, Allocator>::iterator it_;

   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using reference = T &;

    Iterator(Rotation *rotation) : list_{&rotation->list_}, it_{rotation->list_.begin()} {}

    Iterator &reset() {
      this->it_ = this->list_->begin();
      return *this;
    }

    reference operator*() { return *this->it_; }
    pointer operator->() { return &(*this->it_); }

    Iterator &operator++() {
      if (this->list_->empty())
        return *this;

      ++this->it_;
      if (this->it_ == this->list_->end()) {
        this->it_ = this->list_->begin();
      }
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp{*this};
      ++(*this);
      return tmp;
    }

    Iterator &operator--() {
      if (this->list_->empty())
        return *this;

      if (this->it_ == this->list_->begin()) {
        this->it_ = this->list_->end();
      }
      --this->it_;
      return *this;
    }

    Iterator operator--(int) {
      Iterator tmp{*this};
      --(*this);
      return tmp;
    }

    bool operator==(const Iterator &that) const { return this->it_ == that.it_ && this->list_ == that.list_; }

    bool operator!=(const Iterator &that) const { return !(*this == that); }
  };

  void add(T item) { this->list_.push_back(item); }

  std::list<T, Allocator> list_;
};

}  // namespace _rotation
}  // namespace esphome
