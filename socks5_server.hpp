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
    unsigned short listen_port_ = 7000;
    lib::tcp::resolver::results_type target_server_ep_;
public:
    server(unsigned short port):
        listen_port_{port} {}

    lib::awaitable<void> run()
    {
        auto executor = co_await lib::this_coro::executor();
        auto token    = co_await lib::this_coro::token();

        lib::tcp::acceptor acceptor(executor.context(), {lib::tcp::v4(), listen_port_});
        std::cout << "socks5 server start listining on " << listen_port_ << "\n";
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
