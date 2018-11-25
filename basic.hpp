#ifndef BASIC_HPP_
#define BASIC_HPP_

#pragma once

#include <boost/asio/experimental.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/asio.hpp>
#include <boost/scope_exit.hpp>

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
    return *resolver.resolve(server_host, server_port);
}

}// namespace util

}// namespace pika

#endif // BASIC_HPP_
