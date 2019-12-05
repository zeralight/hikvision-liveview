#ifndef DEF_NETWORK_REQUESTS_H
#define DEF_NETWORK_REQUESTS_H

#include <chrono>
#include <functional>
#include <utility>

#include "synchronized_ostream.h"

namespace app {

class network_requests
{

    unsigned long long requests_count_ = 0;
    unsigned long long duration_last_request_;
    unsigned long long requests_sum_ = 0;
    unsigned long long average_time_;


    public:

    template<typename Callable, typename... Args>
    using request_return_t = decltype(std::declval<Callable>() (std::declval<Args>()...));

    template<typename Callable, typename... Args>
    request_return_t<Callable, Args...> request(Callable f, Args... args)
    {
        using namespace std::chrono;
        clog.log("Invoking ", get_function_name(f));
        high_resolution_clock::time_point t1 = high_resolution_clock::now();
        auto ret = f(args...);
        high_resolution_clock::time_point t2 = high_resolution_clock::now();
        
        duration_last_request_ = duration_cast<milliseconds>(t2-t1).count();
        ++requests_count_;
        requests_sum_ += duration_last_request_;
        average_time_ = requests_sum_ / requests_count_;

        clog.log("Request took ", duration_last_request_, " ms");

        return ret;
    }

    const decltype(duration_last_request_)& elapsed_time() const
    {
        return duration_last_request_;
    }

    const decltype(average_time_)& average_time() const
    {
        return average_time_;
    }
};

template<typename Callable, typename... Args>
using request_return_t = decltype(std::declval<Callable>() (std::declval<Args>()...));

template<typename Callable, typename... Args>
request_return_t<Callable, Args...> network_request(Callable f, Args... args)
{
    using namespace std::chrono;
    clog.log("Invoking ", get_function_name(f));
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    auto ret = f(args...);
    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(t2-t1).count();
    clog.log("Request took ", duration, " ms");

    return ret;
}

template<typename Callable, typename... Args>
void network_request_async(Callable f, Args... args)
{
    using namespace std::chrono;
    std::thread([=]()
    {
        clog.log("Invoking ", get_function_name(f));
        high_resolution_clock::time_point t1 = high_resolution_clock::now();
        f(args...);
        high_resolution_clock::time_point t2 = high_resolution_clock::now();

        auto duration = duration_cast<milliseconds>(t2-t1).count();
        clog.log("Request ", get_function_name(f), " took ", duration, " ms");
    }).detach();
}

}

#endif