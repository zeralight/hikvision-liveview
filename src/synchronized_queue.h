#ifndef DEF_SYNCHRONIZED_QUEUE
#define DEF_SYNCHRONIZED_QUEUE

#include <mutex>
#include <queue>

template <typename T>
class synchronized_queue {
 private:
  std::queue<T> queue_;
  std::mutex mutex_;

 public:
  size_t size() const {
    mutex_.lock();
    size_t res = queue_.size();
    mutex_.unlock();

    return res;
  }

  bool empty() const {
    mutex_.lock();
    bool res = queue_.empty();
    mutex_.unlock();

    return res;
  }

  void push(const T& val) {
    mutex_.lock();
    queue_.push(val);
    mutex_.unlock();
  }

  T front() {
    mutex_.lock();
    T res = queue_.front();
    mutex_.unlock();
    return res;
  }

  T back() {
    mutex_.lock();
    T res = queue_.back();
    mutex_.unlock();
    return res;
  }

  void pop() {
    mutex_.lock();
    queue_.pop();
    mutex_.unlock();
  }

};

#endif