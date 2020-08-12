// BeaconClient.cpp : Defines the entry point for the application.
//

#include <iostream>
#include <vector>
#include <stdlib.h>

#include "boost/asio.hpp"
#include "boost/array.hpp"
#include <boost/asio/ssl.hpp>

#include <json/json.h>
#include <json/reader.h>

#define BROADCAST_PORT 1234
#define P2P_PORT 4321
#define MAX_LENGTH 3000

namespace ba = boost::asio;
static Json::Reader reader;

std::string getData(boost::asio::io_context& io_context, const ba::ip::udp::endpoint& broadcastSource)
{
    ba::ip::udp::socket socket(io_context);
    boost::system::error_code ec;
    socket.open(ba::ip::udp::v4());

    std::string request = "Give server list pls";
    size_t request_length = request.length();

    ba::ip::udp::endpoint destination = ba::ip::udp::endpoint(broadcastSource.address(), P2P_PORT);
    socket.send_to(boost::asio::buffer(request, request_length), destination, 0, ec);

    if (ec) {
        std::cout << "CLIENT: Send failed: " << ec.message() << "\n";
    }

    std::cout << "CLIENT: Request sent! " << std::endl;

    boost::array<char, MAX_LENGTH> recv_buf;
    ba::ip::udp::endpoint sender_endpoint;
    size_t len = socket.receive_from(
        boost::asio::buffer(recv_buf), sender_endpoint);

    std::string data(recv_buf.c_array(), len);
    std::cout << "CLIENT: Received data length: " << len << std::endl;
    return data;
}

void displayData(const std::string data) {
    Json::Value root;
    std::string doc = data;

    const char* begin = doc.c_str();
    const char* end = begin + doc.length();

    bool parsingSuccessful = reader.parse(begin, end, root);
    if (!parsingSuccessful) {
        std::cout << "CLIENT: Json parsing failed" << std::endl;
    }

    std::vector<std::pair<int, const char*>> servers;
    servers.reserve(root.size());

    const char* buffer;

    assert(root.isArray());
    for (int i = 0; i < root.size(); ++i) {
        int d = root[i]["distance"].asInt();
        buffer = root[i]["name"].asCString();
        servers.emplace_back(d, buffer);
    }

    std::sort(servers.begin(), servers.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
        });
    
    std::cout << "\nCLIENT: List of servers sorted by their distance:\n";
    for (const auto& pair : servers) {
        std::cout << "Server: " << pair.second;
        std::cout << ", distance: " << pair.first << std::endl;
    }
}

int main() {
	ba::io_context io_context;

    try {
        // In case we get unexpected packet we want to setup again
        while (true) {
            auto socket = ba::ip::udp::socket(io_context,
                ba::ip::udp::endpoint(ba::ip::udp::v4(), BROADCAST_PORT));
            boost::array<char, 100> recv_buf;
            ba::ip::udp::endpoint remote_endpoint;
            boost::system::error_code ec;
            socket.receive_from(boost::asio::buffer(recv_buf),
                remote_endpoint, 0, ec);

            if (ec && ec != boost::asio::error::message_size) {
                std::cout << "CLIENT: Error occured while receiving data: " << ec.message() << std::endl;
            }


            if (std::string(recv_buf.c_array()) != "I am still here") {
                continue;
            }

            std::cout << "CLIENT: Connecting to server..." << std::endl;

            std::string data = getData(io_context, remote_endpoint);

            displayData(data);

            io_context.run();
            std::this_thread::sleep_for(std::chrono::seconds(200));
            break;
        }
    } catch ( ... ) {
        std::exception_ptr eptr = std::current_exception();
    }

    io_context.run();

	return 0;

}
