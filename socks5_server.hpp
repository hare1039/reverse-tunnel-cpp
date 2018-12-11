#ifndef SOCKS5_SERVER_HPP_
#define SOCKS5_SERVER_HPP_

#pragma once
#include <thread>
#include <iostream>
#include <optional>
#include "socks5_session.hpp"

namespace pika::socks5
{

class server
{
    lib::tcp::endpoint listen_ep_;
    lib::tcp::resolver::results_type target_server_ep_;
public:
    server(std::string_view listen_host, boost::asio::io_context &io_context):
        listen_ep_{util::make_connectable(listen_host, io_context)} {}

    lib::awaitable<void> run()
    {
        auto executor = co_await lib::this_coro::executor();
        auto token    = co_await lib::this_coro::token();

        lib::tcp::acceptor acceptor{executor.context(), listen_ep_};
        std::cout << "socks5 server start listining on " << listen_ep_ << "\n";
        for (;;)
        {
            lib::tcp::socket socket = co_await acceptor.async_accept(token);
            lib::co_spawn(executor,
                          [socket = std::move(socket)]() mutable
                          {
                              return std::make_shared<session>(std::move(socket))->start();
                          },
                          lib::detached);
        }
    }
};

}// namespace pika::socks5


#endif // SOCKS5_SERVER_HPP_
