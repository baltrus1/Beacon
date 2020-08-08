// BeaconServer.cpp : Defines the entry point for the application.
//

#include "BeaconServer.h"

#include <boost/asio.hpp>
#include <memory>

#define BROADCAST_PORT 1234

namespace ba = boost::asio;

class PeriodicBroadcasting {
    int m_interval = 1;
    std::atomic<bool> m_running = false;
    std::unique_ptr<std::thread> m_thread;
public:
    PeriodicBroadcasting(int interval) 
    : m_interval(interval)
    {
    }
    
    ~PeriodicBroadcasting() {
        stop();
    }

    void start(const std::shared_ptr<ba::ip::udp::socket>& socket, ba::const_buffer buffer, const ba::ip::udp::endpoint& remote_endpoint) {
        boost::system::error_code error;
        m_running = true;
        m_thread = std::make_unique<std::thread>(&PeriodicBroadcasting::periodicallySend, this, socket, buffer, remote_endpoint, error);
    }

    void stop() {
        if (m_running) {
            m_running = false;
            m_thread->join();
        }
    }

    void periodicallySend(const std::shared_ptr<ba::ip::udp::socket>& socket, ba::const_buffer& buffer, const ba::ip::udp::endpoint& remote_endpoint, boost::system::error_code& error) {
        while (m_running) {
            try {
                socket->send_to(buffer, remote_endpoint, 0, error);
                if (error.failed()) {
                    std::cerr << "Send failed: " << error.message() << std::endl;
                }
            }
            catch (...) {}
            std::this_thread::sleep_for(std::chrono::seconds(m_interval));
        }
    }

};

int main(int argc, char* argv[])
{
    ba::io_service io_service;

    /*ba::ip::udp::resolver resolver(io_service);
    ba::ip::udp::resolver::query query(ba::ip::host_name(), "");
    ba::ip::udp::resolver::iterator it = resolver.resolve(query);

    ba::ip::address local_addr;

    while (it != ba::ip::udp::resolver::iterator())
    {
        local_addr = (it++)->endpoint().address();
        if (local_addr.is_v4()) {
            break;
        }
    }*/

    boost::system::error_code error;
    std::cout << sizeof(error) << std::endl;

    auto socket = std::make_shared< ba::ip::udp::socket>(io_service);
    std::cout << sizeof(socket) << std::endl;
    socket->open(ba::ip::udp::v4(), error);
    if (error.failed()) {
        std::cerr << "Socket open failed: " << error.message() << std::endl;
    }

    ba::socket_base::broadcast option(true);
    socket->set_option(option);

    ba::ip::udp::endpoint remote_endpoint = ba::ip::udp::endpoint(ba::ip::address_v4::broadcast(), BROADCAST_PORT);
    std::cout << sizeof(remote_endpoint) << std::endl;

    const char* message = "I am still here";
    int len = strlen(message);
    ba::const_buffer buffer = ba::buffer(message, len + 1);

    std::cout << sizeof(buffer) << std::endl;

    PeriodicBroadcasting pb(1);

    pb.start(socket, buffer, remote_endpoint);

    std::this_thread::sleep_for(std::chrono::seconds(10));
    io_service.run();
    return 0;
}
