#include <boost/program_options.hpp>
#include <sstream>
#include "socks5_server.hpp"

int main(int argc, char *argv[])
{
    try
    {
//        namespace po = boost::program_options;
//        unsigned short port = 7000;
//        po::options_description desc("Options");
//        desc.add_options()
//            ("help,h", "Print this help messages")
//            ("port,p", po::value<unsigned short>(&port)->default_value(7000), "listen port")
//            ("via,v",  po::value<std::vector<std::string>>(), "the reverse-tunnel host path")
//            ("final",  po::value<std::string>(), "the target tcp server");
//        po::positional_options_description pos_po;
//        pos_po.add("final", 11);
//        po::variables_map vm;
//
//        po::store(po::command_line_parser(argc, argv)
//                  .options(desc)
//                  .positional(pos_po).run(),
//                  vm);
//        po::notify(vm);
//
//        std::stringstream fullpathss;
//        if (vm.count("via"))
//            for (std::string const & s: vm["via"].as<std::vector<std::string>>())
//                fullpathss << s << " ";
//
//        if (vm.count("final"))
//            fullpathss << vm["final"].as<std::string>() << "\n";
//        std::string fullpath = fullpathss.str();

        boost::asio::io_context io_context;
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto){ io_context.stop(); });

        pika::socks5::server server{7000};
        pika::lib::co_spawn(io_context, [&server]{ return server.run(); }, pika::lib::detached);
//        std::vector<std::thread> p(std::thread::hardware_concurrency());
//        for (std::thread & t : p)
//            t = std::thread ([&io_context]{ io_context.run(); });

        io_context.run();
//        for (std::thread & t : p)
//            t.join();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " <<  e.what() << std::endl;
    }
}
