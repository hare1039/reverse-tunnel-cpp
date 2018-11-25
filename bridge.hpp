#ifndef BRIDGE_HPP_
#define BRIDGE_HPP_

namespace pika
{

class bridge : public std::enable_shared_from_this<bridge>
{
public:
    lib::tcp::socket first_socket_;
    lib::tcp::socket second_socket_;

    bridge (lib::tcp::socket && f, lib::tcp::socket && s) :
        first_socket_{std::move(f)},
        second_socket_{std::move(s)} {}

    lib::awaitable<void> start_transport()
    {
        auto self     = shared_from_this();
        auto executor = co_await lib::this_coro::executor();

        lib::co_spawn(executor, [self]() mutable {
                return self->redir(self->first_socket_,
                                   self->second_socket_);
            }, lib::detached);
        lib::co_spawn(executor, [self]() mutable {
                return self->redir(self->second_socket_,
                                   self->first_socket_);
            }, lib::detached);
    }

private:
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
        catch (boost::system::system_error const & e)
        {
            if (e.code() != boost::asio::error::eof)
                std::cerr << "bridge::redir() exception: " << e.what() << std::endl;
        }
        catch (std::exception const &e)
        {
            std::cerr << "bridge::redir() std exception: " << e.what() << std::endl;
        }
        boost::system::error_code ec;
        from.shutdown(lib::tcp::socket::shutdown_receive, ec);
        to.shutdown(lib::tcp::socket::shutdown_send, ec);
        co_return;
    }
};

} // namespace pika

#endif // BRIDGE_HPP_
