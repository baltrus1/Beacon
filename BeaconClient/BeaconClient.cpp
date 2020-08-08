// BeaconClient.cpp : Defines the entry point for the application.
//

#include "BeaconClient.h"

#include "boost/asio.hpp"
#include "boost/array.hpp"

#define BROADCAST_PORT 1234

int main()
{
	namespace ba = boost::asio;

	ba::io_service io_service;

    try
    {
        while(true)
        {
            auto socket = ba::ip::udp::socket(io_service, 
                ba::ip::udp::endpoint(ba::ip::udp::v4(), BROADCAST_PORT));
            boost::array<char, 100> recv_buf;
            ba::ip::udp::endpoint remote_endpoint;
            boost::system::error_code error;
            socket.receive_from(boost::asio::buffer(recv_buf),
                remote_endpoint, 0, error);

            if (error && error != boost::asio::error::message_size)
                throw boost::system::system_error(error);

            std::cout << "Data received: " << std::endl;
            std::cout << recv_buf.c_array() << std::endl;
            /*std::string message = "hello from client";

            boost::system::error_code ignored_error;
            socket.send_to(boost::asio::buffer(message),
                remote_endpoint, 0, ignored_error);*/
        }
    } catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

	return 0;

}
