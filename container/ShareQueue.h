/*!
 * group: GroupName
 * author: Xu Hua
 * date: 2018
 * Contact: xh2010wzx@163.com
 * class: SharedQueue
 * version 1.0
 * brief: 
 *
 * TODO: long description
 *
 * note: The main codes from the website, I just add the "pop_front_value" interface. 
 *		 And this class can't use as function's parameter by reference "&". 
 */
#pragma once
#ifndef _SHAREQUEUE_H_
#define _SHAREQUEUE_H_

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class SharedQueue
{
  public:
    SharedQueue();
    ~SharedQueue();

    T &front();
    void pop_front();
    T &pop_front_value();

    void push_back(const T &item);
    void push_back(T &&item);

    int size();
    bool empty();

  private:
    std::deque<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

template <typename T>
SharedQueue<T>::SharedQueue() {}

template <typename T>
SharedQueue<T>::~SharedQueue() {}

template <typename T>
T &SharedQueue<T>::front()
{
    std::unique_lock<std::mutex> mlock(mutex_);
    cond_.wait(mlock, [this] { return !queue_.empty(); });
    return queue_.front();
}

template <typename T>
void SharedQueue<T>::pop_front()
{
    std::unique_lock<std::mutex> mlock(mutex_);
    cond_.wait(mlock, [this] { return !queue_.empty(); });
    queue_.pop_front();
}

template <typename T>
T &SharedQueue<T>::pop_front_value()
{
    std::unique_lock<std::mutex> mlock(mutex_);
    cond_.wait(mlock, [this] { return !queue_.empty(); });
    T data = queue_.front();
    queue_.pop_front();
    return data;
}

template <typename T>
void SharedQueue<T>::push_back(const T &item)
{
    {
        std::lock_guard<std::mutex> mlock(mutex_);
        queue_.push_back(item);
    }
    cond_.notify_one(); // notify one waiting thread
}

template <typename T>
void SharedQueue<T>::push_back(T &&item)
{
    {
        std::lock_guard<std::mutex> mlock(mutex_);
        queue_.push_back(std::move(item));
    }
    cond_.notify_one(); // notify one waiting thread
}

template <typename T>
int SharedQueue<T>::size()
{
    std::lock_guard<std::mutex> mlock(mutex_);
    return queue_.size();
}

template <typename T>
bool SharedQueue<T>::empty()
{
    std::lock_guard<std::mutex> mlock(mutex_);
    return queue_.empty();
}

#endif
