#ifndef BASIC_HPP_
#define BASIC_HPP_

#pragma once

#include <boost/asio/experimental/co_spawn.hpp>
#include <boost/asio/experimental/detached.hpp>
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

}// namespace pika

#endif // BASIC_HPP_
