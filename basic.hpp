#ifndef BASIC_HPP_
#define BASIC_HPP_

#pragma once

#include <boost/asio/experimental.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/asio.hpp>
#include <boost/scope_exit.hpp>
#include <chrono>

namespace pika
{

namespace lib {

using boost::asio::ip::tcp;
using boost::asio::experimental::co_spawn;
using boost::asio::experimental::detached;
namespace this_coro = boost::asio::experimental::this_coro;

template <typename T>
using awaitable = boost::asio::experimental::awaitable<T, boost::asio::io_context::executor_type>;

}// namespace lib

namespace def
{

constexpr int bufsize = 4 * 1024;

}// namespace def

namespace util
{

inline
std::size_t hash(boost::asio::ip::tcp::endpoint const &e)
{
    std::ostringstream stream;
    stream << e;
    std::hash<std::string> hasher;
    return hasher(stream.str());
}

inline
lib::tcp::endpoint make_connectable(std::string_view host, boost::asio::io_context &io_context)
{
    auto it = std::find(host.begin(), host.end(), ':');
    std::string_view server_host = host.substr(0, std::distance(host.begin(), it));
    std::string_view server_port = host.substr(std::distance(host.begin(), it) + 1, std::distance(it, host.end()) - 1);

    lib::tcp::resolver resolver{io_context};
    if (server_host.empty())
        server_host = "0.0.0.0";
    return *resolver.resolve(server_host, server_port);
}

}// namespace util

namespace error
{
    using namespace std::chrono_literals;
    class restart_request : public std::exception
    {
        constexpr static std::chrono::seconds max_waittime_ {600s};
        std::chrono::seconds waittime_ {600s};
        bool empty_ {true};
    public:
        restart_request() = default;
        restart_request(std::chrono::seconds time):
            waittime_{(time > max_waittime_? max_waittime_ : time)},
            empty_{false} {}

        virtual char const * what() const noexcept override { return "Restart Requested\n"; }
        void sleep() const {std::this_thread::sleep_for(waittime_);}
        operator bool() {return not empty_;};
    };
} // namespace error

}// namespace pika

#endif // BASIC_HPP_
