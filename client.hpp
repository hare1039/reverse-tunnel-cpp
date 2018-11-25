#ifndef CLIENT_HPP_
#define CLIENT_HPP_

#pragma once

#include "basic.hpp"

namespace pika
{

class client : public std::enable_shared_from_this<client>
{
    lib::tcp::endpoint export_ep_;
    lib::tcp::endpoint controller_ep_;

public:
    client(std::string_view export_host, boost::asio::io_context &io_context):
        export_ep_{util::make_connectable(export_host, io_context)} {}

    lib::awaitable<void> run(std::string_view controller_host, std::string_view controller_bind)
    {
        try
        {
            auto executor = co_await lib::this_coro::executor();
            auto token    = co_await lib::this_coro::token();
            auto self     = shared_from_this();

            self->controller_ep_ = util::make_connectable(controller_host, executor.context());
            lib::tcp::socket controller_socket{executor.context()};
            co_await controller_socket.async_connect(self->controller_ep_, token);

            auto controller_bind_ep = util::make_connectable(controller_bind, executor.context());
            { // send bind request
                std::uint32_t ip   = controller_bind_ep.address().to_v4().to_ulong();
                boost::endian::native_to_big_inplace(ip);
                std::uint16_t port = controller_bind_ep.port();
                boost::endian::native_to_big_inplace(port);

                std::array<std::uint8_t, 8> req{0x01, 0x00};
                std::memcpy(&req[2]            , &ip,   sizeof ip);
                std::memcpy(&req[2 + sizeof ip], &port, sizeof port);
                std::ignore = co_await boost::asio::async_write(controller_socket, boost::asio::buffer(req), token);
            }

            for (;;)
            {
                std::array<std::uint8_t, 8> buf{};
                std::size_t length = co_await boost::asio::async_read(controller_socket, boost::asio::buffer(buf), token);

                switch(buf.at(0))
                {
                    case 0x00: // do nothing
                        break;
                    case 0x03: // Is remote request
                    {
                        std::uint32_t id = 0;
                        std::memcpy(&id, &buf[2], 4);
                        lib::co_spawn(executor,
                                      [self, id]() mutable {
                                          return self->make_bridge(id);
                                      }, lib::detached);
                        break;
                    }
                    default:
                        // response failed
                        break;
                    }
            }

        }
        catch (std::exception const & e)
        {
            std::cerr << "session::start() exception: " << e.what() << std::endl;
        }
    }

    lib::awaitable<void> make_bridge(std::uint32_t const id)
    {
        try
        {
            auto executor = co_await lib::this_coro::executor();
            auto token    = co_await lib::this_coro::token();
            auto self     = shared_from_this();

            auto proxy_bridge = std::make_shared<bridge>(lib::tcp::socket{executor.context()},
                                                   lib::tcp::socket{executor.context()});
            lib::tcp::socket
                & export_socket{proxy_bridge->first_socket_},
                & controller_socket{proxy_bridge->second_socket_};
            co_await export_socket.async_connect(self->export_ep_, token);
            co_await controller_socket.async_connect(self->controller_ep_, token);
            std::array<std::uint8_t, 8> req{0x02, 0x00};
            std::memcpy(&req[2], &id, sizeof id);
            std::ignore = co_await boost::asio::async_write(controller_socket, boost::asio::buffer(req), token);
            co_await proxy_bridge->start_transport();
        }
        catch (std::exception const & e)
        {
            std::cerr << "client::make_bridge() exception: " << e.what() << std::endl;
        }
    }
};

}

#endif // CLIENT_HPP_
