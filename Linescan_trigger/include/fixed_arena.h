#pragma once

#include <stddef.h>
#include <stdint.h>

template <size_t Capacity>
class FixedArena {
 public:
  void reset() {
    offset_ = 0;
  }

  void *allocate(size_t size, size_t alignment = alignof(max_align_t)) {
    const size_t aligned = alignUp(offset_, alignment);
    if (aligned + size > Capacity) {
      return nullptr;
    }

    void *ptr = &buffer_[aligned];
    offset_ = aligned + size;
    return ptr;
  }

  size_t used() const {
    return offset_;
  }

  size_t capacity() const {
    return Capacity;
  }

 private:
  static size_t alignUp(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
  }

  alignas(max_align_t) uint8_t buffer_[Capacity] = {};
  size_t offset_ = 0;
};
