#ifndef DEF_CONSUMER_H
#define DEF_CONSUMER_H

#include <deque>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "synchronized_ostream.h"

namespace app {

template <typename Func, typename AccumulateFunc, typename... Args>
class Consumer {
  using ContainerType = std::queue<std::tuple<Args...>>;

  Func function_;
  AccumulateFunc accumulate_func_;
  std::mutex mutex_;
  std::condition_variable condition_to_consume_;
  ContainerType queue_;
  bool blocked_;

  template <typename Tuple, size_t... I>
  void invoke_function(const Tuple& args, std::index_sequence<I...>) {
    function_(std::get<I>(args)...);
  }

 public:
  Consumer(Func&& f, AccumulateFunc&& af) : function_(f), accumulate_func_(af), blocked_(false) {}

  const ContainerType& queue() const { return queue_; }

  std::mutex& mutex() { return mutex_; }
  const std::mutex& mutex() const { return mutex_; }

  void block_process() {
    std::unique_lock<std::mutex> lock(mutex_);
    blocked_ = true;
  }
  void unblock_process() {
    std::unique_lock<std::mutex> lock(mutex_);
    blocked_ = false;
    lock.unlock();
    condition_to_consume_.notify_all();
  }

  void run() {
    std::thread([this]() {
      clog.log("Started running");
      while (true) {
        // clog.log("Waiting for an element");
        std::unique_lock<std::mutex> locker(mutex_);
        condition_to_consume_.wait(locker, [this]() { return !queue().empty() && !blocked_; });
        clog.log("Consumer::run: processing ", queue_.size(), " elements");
        auto&& val = accumulate_func_(queue_);
        auto new_queue = std::queue<std::tuple<Args...>>();
        std::swap(queue_, new_queue);
        locker.unlock();
        invoke_function(val, std::index_sequence_for<Args...>{});
      }
    })
        .detach();
  }

  void queue(Args... args) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      clog.log("Consumer::queue: Adding to queue one element: [", args..., "]");
      queue_.push(std::make_tuple(args...));
    }
    condition_to_consume_.notify_all();
  }
};

}  // namespace app

#endif