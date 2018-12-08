#include <boost/program_options.hpp>
#include <sstream>
#include "socks5_server.hpp"
#include "controller.hpp"
#include "client.hpp"

int main(int argc, char *argv[])
{
    try
    {
        namespace po = boost::program_options;
        enum class mode {
            srv,
            exp,
            socks5
        };
        mode run_mode {mode::srv};
        unsigned short port {7000}, socks5_port {0};
        std::string connect, exp, bind;
        std::unique_ptr<pika::lib::tcp::socket> socks5_server_endpoint_socket {nullptr};
        boost::asio::io_context io_context;

        po::options_description desc{"Options"};
        desc.add_options()
            ("help,h", "Print this help messages")
            ("port,p",    po::value<unsigned short>(&port)->default_value(7000), "[server mode] listen port")
            ("connect,c", po::value<std::string>(), "[export mode] connect to server")
            ("export,e",  po::value<std::string>(), "[export mode] export server endpoint")
            ("bind,b",    po::value<std::string>(), "[export mode] bind remote server")
            ("socks5,s",  po::value<unsigned short>(), "[socks5 mode] start socks5 server on this port");
        po::positional_options_description pos_po;
        po::variables_map vm;

        po::store(po::command_line_parser(argc, argv)
                  .options(desc)
                  .positional(pos_po).run(),
                  vm);
        po::notify(vm);
        if (vm.count("connect") || vm.count("export") || vm.count("bind"))
        {
            run_mode = mode::exp;
            if ((!! vm.count("connect")) ^ (!! vm.count("bind")))
            {
                std::cerr << "[export mode] --connect and and --bind must spectify at the same time\n";
                std::exit(1);
            }

            connect = vm["connect"].as<std::string>();
            bind    = vm["bind"].as<std::string>();

            if (not vm.count("export"))
            {
                std::cout << "[export mode] --export not present, using internal socks5 server\n";
                socks5_server_endpoint_socket = std::make_unique<pika::lib::tcp::socket>(io_context,
                                                                                         pika::lib::tcp::endpoint{pika::lib::tcp::v4(), 0});

                using namespace std::literals;
                exp = "0.0.0.0:"s + std::to_string(socks5_server_endpoint_socket->local_endpoint().port());
            }
            else
                exp = vm["export"].as<std::string>();
        }
        else if (vm.count("socks5"))
        {
            run_mode = mode::socks5;
            socks5_port = vm["socks5"].as<unsigned short>();
        }
        else
            run_mode = mode::srv;

        boost::asio::signal_set signals{io_context, SIGINT, SIGTERM};
        signals.async_wait([&](auto, auto){ io_context.stop(); });

        switch (run_mode)
        {
            case mode::socks5:
            {
                std::cout << "[socks5 mode] Starting socks5 server at localhost:" << socks5_port << "\n";
                pika::socks5::server server{socks5_port};
                pika::lib::co_spawn(io_context,
                                    [&server] {
                                        return server.run();
                                    }, pika::lib::detached);
                io_context.run();
                break;
            }
            case mode::srv:
            {
                pika::controller server{port};
                pika::lib::co_spawn(io_context, [&server]{ return server.run(); }, pika::lib::detached);
                io_context.run();
                break;
            }
            case mode::exp:
            {
                // if socks5_server_endpoint_socket not null, start socks5 server
                if (socks5_server_endpoint_socket)
                {
                    std::thread t(
                        [socket = std::move(socks5_server_endpoint_socket)]() mutable
                        {
                            // retrive the random port from socket, release it, than bind it again
                            unsigned short port = socket->local_endpoint().port();
                            std::cout << "Starting socks5 server at localhost:" << port << "\n";
                            socket.reset();
                            pika::socks5::server server{port};
                            boost::asio::io_context io;
                            pika::lib::co_spawn(io,
                                                [&server] {
                                                    return server.run();
                                                }, pika::lib::detached);
                            io.run();
                        });
                    t.detach();
                }
                auto c = std::make_shared<pika::client>(exp, io_context);
                pika::lib::co_spawn(io_context,
                                    [&c, &connect, &bind] {
                                        return c->run(connect, bind);
                                    }, pika::lib::detached);
                io_context.run();
                break;
            }
        }
//        std::vector<std::thread> p(std::thread::hardware_concurrency());
//        for (std::thread & t : p)
//            t = std::thread ([&io_context]{ io_context.run(); });
//        for (std::thread & t : p)
//            t.join();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " <<  e.what() << std::endl;
    }
}
