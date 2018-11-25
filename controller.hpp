#ifndef CONTROLLER_HPP_
#define CONTROLLER_HPP_

#include <unordered_map>
#include <memory>
#include "basic.hpp"

namespace pika
{

class controller
{
    unsigned short listen_port_ = 7000;
    std::unordered_map<std::uint32_t, lib::tcp::socket> clients;
public:
    controller(unsigned short port):
    listen_port_{port} {}

    lib::awaitable<void> run()
    {
        auto executor = co_await lib::this_coro::executor();
        auto token    = co_await lib::this_coro::token();

        lib::tcp::acceptor acceptor(executor.context(), {lib::tcp::v4(), listen_port_});
        std::cout << "start listining on " << listen_port_ << "\n";
        for (;;)
        {
            lib::tcp::socket socket = co_await acceptor.async_accept(token);
            std::array<std::uint8_t, 8> buf;
            std::size_t length = co_await boost::asio::async_read(socket, boost::asio::buffer(buf), token);
            assert(length == 8);

            switch(buf.at(0))
            {
                case 0x00: // do nothing
                    break;
                case 0x01: // Request bind port
                {
                    std::uint32_t ipv4 = 0;
                    std::memcpy(&ipv4, &buf[2], sizeof ipv4);
                    boost::endian::big_to_native_inplace(ipv4);

                    std::uint16_t port = 0;
                    std::memcpy(&port, &buf[2 + sizeof ipv4], sizeof port);
                    boost::endian::big_to_native_inplace(port);

                    lib::co_spawn(executor,
                                  [socket = std::move(socket), ipv4, port, this]() mutable {
                                      boost::asio::socket_base::keep_alive opt{true};
                                      socket.set_option(opt);
                                      return start_reverse_tunnel(std::move(socket), ipv4, port);
                                  }, lib::detached);
                    break;
                }
                case 0x02: // Connect with id
                {
                    std::uint32_t id = 0;
                    std::memcpy(&id, &buf[2], sizeof id);
                    boost::endian::big_to_native_inplace(id);
                    lib::co_spawn(executor,
                                  [socket = std::move(socket), id, this]() mutable {
                                      return start_bridge(std::move(socket), id);
                                  }, lib::detached);
                    break;
                }
                default:
                    // response failed
                    break;
            }
        }
    }
private:
    lib::awaitable<void> start_reverse_tunnel(lib::tcp::socket && remote_socket,
                                              std::uint32_t ip, std::uint16_t port)
    {
        auto executor = co_await lib::this_coro::executor();
        auto token    = co_await lib::this_coro::token();

        try
        {
            lib::tcp::endpoint ep{boost::asio::ip::address_v4{ip}, port};
            lib::tcp::acceptor acceptor(executor.context(), ep);
            std::cout << "reverse tunnel start listening on " << ep << "\n";
            BOOST_SCOPE_EXIT (&ep) {
                std::cout << "reverse tunnel closed listening on " << ep << "\n";
            } BOOST_SCOPE_EXIT_END;

            lib::co_spawn(executor,
                          [&remote_socket, &acceptor]() mutable {
                              return monitor_socket(remote_socket, acceptor);
                          }, lib::detached);

            for (;;)
            {
                lib::tcp::socket socket = co_await acceptor.async_accept(token);
                std::uint32_t address   = util::hash(socket.remote_endpoint());
                clients.insert({address, std::move(socket)});

                std::array<std::uint8_t, 8> response{0x03};
                boost::endian::native_to_big_inplace(address);
                std::memcpy(&response[2], &address, sizeof address);

                std::ignore = co_await boost::asio::async_write(remote_socket, boost::asio::buffer(response), token);
            }
        }
        catch (std::exception const & e)
        {
            std::cerr << "controller::start_reverse_tunnel exception: " << e.what() << std::endl;
        }
    }

    static
    lib::awaitable<void> monitor_socket(lib::tcp::socket & remote, lib::tcp::acceptor & acceptor)
    {
        std::array<std::uint8_t, 8> keep_alive{};
        auto executor = co_await lib::this_coro::executor();
        auto token    = co_await lib::this_coro::token();
        using namespace std::literals;

        try
        {
            for (;;)
            {
                boost::asio::steady_timer t{executor.context(), 10s};
                co_await t.async_wait(token);
                std::ignore = co_await boost::asio::async_write(remote, boost::asio::buffer(keep_alive), token);
            }
        }
        catch(boost::system::system_error const & e)
        {
            if (e.code() != boost::asio::error::eof ||
                e.code() != boost::asio::error::broken_pipe)
                std::cerr << "controller::monitor_socket exception: " << e.what() << std::endl;
            acceptor.cancel();
            acceptor.close();
        }
    }

    lib::awaitable<void> start_bridge(lib::tcp::socket && s, std::uint32_t id)
    {
        try
        {
            lib::tcp::socket &local = clients.at(id);
            auto b = std::make_shared<bridge>(std::move(s), std::move(local));
            controller * self = this;
            BOOST_SCOPE_EXIT (self, id) {
                self->clients.erase(id);
            } BOOST_SCOPE_EXIT_END;
            co_await b->start_transport();
        }
        catch (std::exception const & e)
        {
            std::cerr << "controller::start_bridge exception: " << e.what() << std::endl;
        }
    }
};

}// namespace pika

#endif // CONTROLLER_HPP_
