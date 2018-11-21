#ifndef SOCKS5_SESSION_HPP_
#define SOCKS5_SESSION_HPP_

#pragma once

#include <set>
#include <deque>
#include <string_view>
#include "basic.hpp"

namespace pika::socks5
{

class session : public std::enable_shared_from_this<session>
{
    lib::tcp::socket socket_;
    lib::tcp::socket target_socket_;

public:
    session(lib::tcp::socket && client):
        socket_{std::move(client)},
        target_socket_{socket_.get_executor().context()}{}

    lib::awaitable<void> start()
    {
        try
        {
            auto self     = shared_from_this();
            auto token    = co_await lib::this_coro::token();
            auto executor = co_await lib::this_coro::executor();

            BOOST_SCOPE_EXIT (self) {
                std::cout << "close session #" << self->id() << std::endl;
            } BOOST_SCOPE_EXIT_END;

            { // socks5 handshake
                /*
                 +----+----------+----------+
                 |VER | NMETHODS | METHODS  |
                 +----+----------+----------+
                 | 1  |    1     | 1 to 255 |
                 +----+----------+----------+
                 o  X'00' NO AUTHENTICATION REQUIRED
                 o  X'01' GSSAPI
                 o  X'02' USERNAME/PASSWORD
                 o  X'03' to X'7F' IANA ASSIGNED
                 o  X'80' to X'FE' RESERVED FOR PRIVATE METHODS
                 o  X'FF' NO ACCEPTABLE METHODS
                */
                std::array<std::uint8_t, 2> buf; // buf contains {VER, NMETHODS}
                std::size_t length = co_await boost::asio::async_read(socket_, boost::asio::buffer(buf), token);
                assert(length == 2);

                if (buf[0] != 0x05)
                    throw std::runtime_error("Protocol mismatch");

                std::uint8_t num = buf[1];
                std::vector<std::uint8_t> nmethod_buf(num);
                length = co_await boost::asio::async_read(socket_, boost::asio::buffer(nmethod_buf), token);
                assert(length == num);

                std::array<std::uint8_t, 2> response{{0x05, 0xFF}};

                for (int i = 0; i < num; i++)
                    if (nmethod_buf.at(i) == 0x00 /* NO AUTHENTICATION REQUIRED */)
                        response[1] = 0x00;

                std::ignore = co_await boost::asio::async_write(socket_, boost::asio::buffer(response), token);
            } // socks5 handshake end

            lib::tcp::endpoint target_endpoint;
            { // socks5 request
                /* array<std::uint8_t, 4> head{VER, CMD, RSV, ATYP}
                 +----+-----+-------+------+----------+----------+
                 |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
                 +----+-----+-------+------+----------+----------+
                 | 1  |  1  | X'00' |  1   | Variable |    2     |
                 +----+-----+-------+------+----------+----------+
                 o  VER    protocol version: X'05'
                 o  CMD
                    o  CONNECT X'01'
                    o  BIND    X'02'
                    o  UDP ASSOCIATE X'03'
                 o  RSV    RESERVED
                 o  ATYP   address type of following address
                    o  IP V4 address: X'01'
                    o  DOMAINNAME:    X'03'
                    o  IP V6 address: X'04'
                 o  DST.ADDR       desired destination address
                 o  DST.PORT desired destination port in network octet
                    order
                 */
                std::array<std::uint8_t, 4> head{}; // field {VER, CMD, RSV, ATYP}
                std::size_t length = co_await boost::asio::async_read(socket_, boost::asio::buffer(head), token);
                assert(length == 4);
                enum {
                    VER = 0,
                    CMD,
                    RSV,
                    ATYP
                };

                if (head[VER] != 0x05 ||
                    head[CMD] != 0x01 /* CONNECT */)
                    throw std::runtime_error("socks5 request invalid");

                int constexpr port_length = 2;
                switch (head[ATYP])
                {
                    case 0x01 /* IP v4 */:
                    {
                        // version-4 IP address, with a length of 4 octets
                        std::array<std::uint8_t, 4 + port_length> buf{};
                        length = co_await boost::asio::async_read(socket_, boost::asio::buffer(buf), token);
                        assert(length == 4 + port_length);
                        std::uint32_t n_ip = 0;
                        static_assert(sizeof(std::uint32_t) == 4);
                        std::memcpy(&n_ip, buf.data(), sizeof n_ip);
                        boost::endian::big_to_native_inplace(n_ip);
                        boost::asio::ip::address_v4 ipv4{n_ip};

                        std::uint16_t n_port = 0;
                        std::memcpy(&n_port, buf.data() + 4, port_length);
                        boost::endian::big_to_native_inplace(n_port);

                        target_endpoint = lib::tcp::endpoint(ipv4, n_port);
                        break;
                    }
                    case 0x03 /* domain name */:
                    {
                        // get length of domain name
                        std::array<std::uint8_t, 1> buf{};
                        std::ignore = co_await boost::asio::async_read(socket_, boost::asio::buffer(buf), token);
                        int domain_name_length = buf[0];
                        std::vector<std::uint8_t> domain(domain_name_length + port_length);
                        length = co_await boost::asio::async_read(socket_, boost::asio::buffer(domain), token);
                        assert(length == domain_name_length + port_length);

                        std::uint16_t n_port = 0;
                        std::memcpy(&n_port, domain.data() + domain_name_length, port_length);
                        boost::endian::big_to_native_inplace(n_port);

                        std::string domain_name;
                        std::copy_n (domain.data(), domain_name_length, std::back_inserter(domain_name));

                        lib::tcp::resolver resolver{socket_.get_executor().context()};
                        lib::tcp::resolver::results_type r = resolver.resolve(domain_name, std::to_string(n_port));

                        target_endpoint = *r;
                        break;
                    }
                    case 0x04 /* IP v6 */:
                    {
                        // version-4 IP address, with a length of 16 octets
                        std::array<std::uint8_t, 16 + port_length> buf{};
                        std::cout << "IP v6 not impl\n";
                        [[fallthrough]];
                    }
                    default:
                        throw std::runtime_error("ATYP not supported");
                }
                std::cout << "target endpoint: " << target_endpoint << "\n";

            } // socks5 request end

            { // response of socks5 request
                /*
                 +----+-----+-------+------+----------+----------+
                 |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
                 +----+-----+-------+------+----------+----------+
                 | 1  |  1  | X'00' |  1   | Variable |    2     |
                 +----+-----+-------+------+----------+----------+
                   o  VER    protocol version: X'05'
                   o  REP    Reply field:
                      o  X'00' succeeded
                      o  X'01' general SOCKS server failure
                      o  X'02' connection not allowed by ruleset
                      o  X'03' Network unreachable
                      o  X'04' Host unreachable
                      o  X'05' Connection refused
                      o  X'06' TTL expired
                      o  X'07' Command not supported
                      o  X'08' Address type not supported
                      o  X'09' to X'FF' unassigned
                   o  RSV    RESERVED
                   o  ATYP   address type of following address
                      o  IP V4 address: X'01'
                      o  DOMAINNAME:    X'03'
                      o  IP V6 address: X'04'
                   o  BND.ADDR       server bound address
                   o  BND.PORT       server bound port in network octet order
                 */
                std::array<std::uint8_t,
                           4 /* header */ +
                           4 /* ipv4 */ +
                           2 /* port */> response{{
                    0x05, // VER
                    0x00, // REP
                    0x00, // RSV
                    0x01, // ATYP == ipv4
                }};
                try
                {
                    co_await target_socket_.async_connect(target_endpoint, token);

                    std::uint32_t ip = target_socket_.remote_endpoint().address().to_v4().to_ulong();
                    boost::endian::native_to_big_inplace(ip);
                    std::uint16_t port = target_socket_.remote_endpoint().port();
                    boost::endian::native_to_big_inplace(port);

                    std::memcpy(&response[4], &ip,   sizeof ip);
                    std::memcpy(&response[8], &port, sizeof port);
                }
                catch (boost::system::system_error const & e)
                {
                    switch (e.code().value())
                    {
                        case boost::asio::error::network_unreachable: response[1] = 0x03; break;
                        case boost::asio::error::host_unreachable:    response[1] = 0x04; break;
                        case boost::asio::error::connection_refused:  response[1] = 0x05; break;
                        case boost::asio::error::timed_out:           response[1] = 0x06; break;
                        default: response[1] = 0x01; break;
                    }
                    std::ignore = co_await boost::asio::async_write(socket_, boost::asio::buffer(response), token);

                    using namespace std::literals;
                    throw std::runtime_error("Bad target endpoint, error: "s + e.what());
                    co_return;
                }
                std::ignore = co_await boost::asio::async_write(socket_, boost::asio::buffer(response), token);
            } // response of socks5 request end

            lib::co_spawn(executor, [self]() mutable {
                              return self->redir(self->socket_, self->target_socket_);
                          }, lib::detached);
            auto && stoc = self->redir(self->target_socket_, self->socket_);

            std::cout << "setup session #" << self->id() << std::endl;
            co_await stoc;
        }
        catch (const std::exception & e)
        {
            std::cerr << "session::start() exception: " << e.what() << std::endl;
        }
        co_return;
    }

    lib::awaitable<void> redir(lib::tcp::socket &from, lib::tcp::socket &to)
    {
        auto executor = co_await lib::this_coro::executor();
        auto token    = co_await lib::this_coro::token();
        auto self     = shared_from_this();
        try
        {
            std::array<char, def::bufsize> raw_buf;
            for (;;)
            {
                std::size_t read_n = co_await from.async_read_some(boost::asio::buffer(raw_buf), token);
                std::ignore = co_await boost::asio::async_write(to, boost::asio::buffer(raw_buf, read_n), token);
            }
        }
        catch (const boost::system::system_error & e)
        {
            if (e.code() != boost::asio::error::eof)
                std::cerr << "session::redir() exception: " << e.what() << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "session::redir() std exception: " << e.what() << std::endl;
        }
        boost::system::error_code ec;
        from.shutdown(lib::tcp::socket::shutdown_receive, ec);
        to.shutdown(lib::tcp::socket::shutdown_send, ec);
        co_return;
    }

    inline
    std::size_t id() { return std::hash<std::shared_ptr<session>>{}(shared_from_this()); }
};

}// namespace pika

#endif // SOCKS5_SESSION_HPP_
