#ifndef SYNCHRONIZED_OSTREAM_H
#define SYNCHRONIZED_OSTREAM_H

#include <iostream>
#include <mutex>
#include <thread>

#include "util.h"

namespace app {
class synchronized_ostream : public std::ostream
{
    std::mutex mutex_;
    bool verbose_;

    using std::ostream::operator<<;

    template<typename Arg, typename... Args>
    synchronized_ostream& log_thread_safe(const Arg& arg, const Args&... args)
    {
        static_cast<std::ostream&>(*this) << arg;
        return log_thread_safe(args...);
    }

    template<typename Arg>
    synchronized_ostream& log_thread_safe(const Arg& arg)
    {
        static_cast<std::ostream&>(*this) << arg;
        return *this;
    }

    public:

    synchronized_ostream(std::ostream& stream, bool verbose=false): std::ostream(stream.rdbuf()), verbose_(verbose) {}

#if !defined(NDEBUG)
    template<typename Arg, typename... Args>
    synchronized_ostream& log(const Arg& arg, const Args&... args)
    {
        std::unique_lock<std::mutex> lock{mutex_};
        if (verbose_)
            static_cast<std::ostream&>(*this) << timeNow() << " - Thread: " << std::this_thread::get_id() << ": ";
        log_thread_safe(arg, args...);
        static_cast<std::ostream&>(*this) << '\n';

        return *this;
    }

    template<typename Arg>
    synchronized_ostream& log(const Arg& arg)
    {
        std::unique_lock<std::mutex> lock{mutex_};
        if (verbose_)
            static_cast<std::ostream&>(*this) << timeNow() << " - Thread: " << std::this_thread::get_id() << ": ";
        log_thread_safe(arg);
        static_cast<std::ostream&>(*this) << '\n';

        return *this;
    }
#else
    template<typename Arg, typename... Args>
    synchronized_ostream& log(const Arg& arg, const Args&... args)
    {
        return *this;
    }

    template<typename Arg>
    synchronized_ostream& log(const Arg& arg)
    {
        return *this;
    }
#endif

};

extern synchronized_ostream clog;
// extern synchronized_ostream cerr;

}

#endif