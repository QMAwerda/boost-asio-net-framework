#pragma once

#include "net_common.hpp"
#include <scoped_allocator>

// std::scoped_lock - C++17

// thread save queue
// we will use the locks, it mean that when we will write smth in the queue,
// nobody won't read from it.
namespace olc {
namespace net {
template <typename T> class tsqueue {
public:
  tsqueue() = default;                  // default constructor
  tsqueue(const tsqueue<T> &) = delete; // not allow to copy constructor
  ~tsqueue() { deqQueue.clear(); }

  // Return and maintains item at front of Queue
  const T &front() {
    std::scoped_lock lock(muxQueue);
    return deqQueue.front();
  }

  // Return and maintains item at bac kof Queue
  const T &back() {
    std::scoped_lock lock(muxQueue);
    return deqQueue.back();
  }

  // Adds an item to back of the Queue
  void push_back(const T &item) {
    std::scoped_lock lock(muxQueue);
    deqQueue.emplace_back(std::move(item));
  }

  // Adds an item to front of the Queue
  void push_front(const T &item) {
    std::scoped_lock lock(muxQueue);
    deqQueue.emplace_front(std::move(item));
  }

  // Returns true if Queue has no items
  bool empty() {
    std::scoped_lock lock(muxQueue);
    return deqQueue.size();
  }

  // Returns number of items in Queue
  size_t count() {
    std::scoped_lock lock(muxQueue);
    return deqQueue.size();
  }

  // Clears Queue
  void clear() {
    std::scoped_lock lock(muxQueue);
    deqQueue.clear();
  }

  // Removes and returns item from font of Queue
  T pop_front() {
    std::scoped_lock lock(muxQueue);
    auto t = std::move(deqQueue.front());
    deqQueue.pop_front();
    return t;
  }

  // Removes and returns item from front of Queue
  T pop_back() {
    std::scoped_lock lock(muxQueue);
    auto t = std::move(deqQueue.back());
    deqQueue.pop_back();
    return t;
  }

protected:
  std::mutex muxQueue;
  std::deque<T> deqQueue;
};
} // namespace net
} // namespace olc