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
            exp
        };
        mode run_mode;
        unsigned short port = 7000, socks5_port = 7001;
        std::string connect, exp, bind;
        bool run_socks5_server = false;
        boost::asio::io_context io_context;

        po::options_description desc("Options");
        desc.add_options()
            ("help,h", "Print this help messages")
            ("port,p",    po::value<unsigned short>(&port)->default_value(7000), "[server mode] listen port")
            ("connect,c", po::value<std::string>(), "[export mode] connect to server")
            ("export,e",  po::value<std::string>(), "[export mode] export server endpoint")
            ("bind,b",    po::value<std::string>(), "[export mode] bind remote server");
        po::positional_options_description pos_po;
        po::variables_map vm;

        po::store(po::command_line_parser(argc, argv)
                  .options(desc)
                  .positional(pos_po).run(),
                  vm);
        po::notify(vm);
        if (vm.count("connect") || vm.count("export") || vm.count("bind"))
            run_mode = mode::exp;
        else
            run_mode = mode::srv;


        if (run_mode == mode::exp)
        {
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
                pika::lib::tcp::socket socket{io_context, {pika::lib::tcp::v4(), 0}};
                socks5_port = socket.local_endpoint().port();

                using namespace std::literals;
                exp = "0.0.0.0:"s + std::to_string(socks5_port);
                run_socks5_server = true;
            }
            else
                exp = vm["export"].as<std::string>();
        }

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto){ io_context.stop(); });

        switch (run_mode)
        {
            case mode::srv:
            {
                pika::controller server{port};
                pika::lib::co_spawn(io_context, [&server]{ return server.run(); }, pika::lib::detached);
                io_context.run();
            }
            case mode::exp:
            {
                if (run_socks5_server)
                {
                    std::thread t(
                        [&socks5_port]()
                        {
                            std::cout << "Starting socks5 server at localhost:" << socks5_port << "\n";
                            pika::socks5::server server{socks5_port};
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
