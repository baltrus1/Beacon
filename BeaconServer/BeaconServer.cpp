// BeaconServer.cpp : Defines the entry point for the application.
//

#include "BeaconServer.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>

#define BROADCAST_PORT 1234
#define MAX_LENGTH 1024

namespace ba = boost::asio;

class PeriodicBroadcasting {
    int m_interval = 1;
    std::atomic<bool> m_running = false;
    std::unique_ptr<std::thread> m_thread;
    boost::system::error_code m_error;
public:
    PeriodicBroadcasting(int interval) 
    : m_interval(interval)
    {
    }
    
    ~PeriodicBroadcasting() {
        stop();
    }

    void start(const std::shared_ptr<ba::ip::udp::socket>& socket, ba::const_buffer buffer, const ba::ip::udp::endpoint& remote_endpoint) {
        m_running = true;
        m_thread = std::make_unique<std::thread>(&PeriodicBroadcasting::periodicallySend, this, socket, buffer, remote_endpoint);
    }

    void stop() {
        if (m_running) {
            m_running = false;
            m_thread->join();
        }
    }

    void periodicallySend(const std::shared_ptr<ba::ip::udp::socket>& socket, ba::const_buffer& buffer, const ba::ip::udp::endpoint& remote_endpoint) {
        while (m_running) {
            try {
                socket->send_to(buffer, remote_endpoint, 0, m_error);
                if (m_error) {
                    std::cerr << "Send failed: " << m_error.message() << std::endl;
                }
            }
            catch (...) {}
            std::this_thread::sleep_for(std::chrono::seconds(m_interval));
        }
    }

};

// modified version of https://www.boost.org/doc/libs/1_73_0/doc/html/boost_asio/example/cpp11/ssl/client.cpp
class client
{
public:
    client(boost::asio::io_context& io_context,
        boost::asio::ssl::context& context,
        const ba::ip::tcp::resolver::results_type& endpoints)
        : socket_(io_context, context)
    {
        socket_.set_verify_mode(boost::asio::ssl::verify_peer);
        socket_.set_verify_callback(
            std::bind(&client::verify_certificate, this, std::placeholders::_1, std::placeholders::_2));

        connect(endpoints);
    }

private:
    bool verify_certificate(bool preverified,
        boost::asio::ssl::verify_context& ctx)
    {
        // The verify callback can be used to check whether the certificate that is
        // being presented is valid for the peer. For example, RFC 2818 describes
        // the steps involved in doing this for HTTPS. Consult the OpenSSL
        // documentation for more details. Note that the callback is called once
        // for each certificate in the certificate chain, starting from the root
        // certificate authority.

        // In this example we will simply print the certificate's subject name.
        /*char subject_name[256];
        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
        std::cout << "Verifying " << subject_name << "\n";

        return preverified;*/

        return true;
    }

    void connect(const ba::ip::tcp::resolver::results_type& endpoints)
    {
        ba::async_connect(socket_.lowest_layer(), endpoints,
            [this](const boost::system::error_code& error,
                const ba::ip::tcp::endpoint& /*endpoint*/)
            {
                if (!error)
                {
                    handshake();
                }
                else
                {
                    std::cout << "Connect failed: " << error.message() << "\n";
                }
            });
    }

    void handshake()
    {
        socket_.async_handshake(boost::asio::ssl::stream_base::client,
            [this](const boost::system::error_code& error)
            {
                if (!error)
                {
                    send_request();
                }
                else
                {
                    std::cout << "Handshake failed: " << error.message() << "\n";
                }
            });
    }

    void send_request()
    {
        std::ostringstream request_stream;
        //std::string json = "{'username' : 'tesonet',  'password' : 'partyanimal' }";
        std::string json = "{\"username\" : \"tesonet\",  \"password\" : \"partyanimal\" }";
        //std::string json = "username=tesonet&password=partyanimal";

        request_stream << "POST /v1/tokens HTTP/1.1 \r\n";
        request_stream << "Host: " << "playground.tesonet.lt" << "\r\n";
        request_stream << "User-Agent: curl/7.55.1" << "\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Content-Type: application/json\r\n";
        request_stream << "Content - Length: " << json.length() << "\r\n\r\n";
        request_stream << json << "\r\n";

        auto request_str = request_stream.str();
        std::cout << "Request: " << std::endl << std::endl << request_str << std::endl << std::endl;

        ba::async_write(socket_,
            boost::asio::buffer(request_str, request_str.length()),
            [this](const boost::system::error_code& error, std::size_t length)
            {
                if (!error)
                {
                    receive_response(length);
                }
                else
                {
                    std::cout << "Write failed: " << error.message() << "\n";
                }
            });
    }

    void receive_response(std::size_t length)
    {
        boost::asio::async_read(socket_,
            boost::asio::buffer(reply_, length),
            [this](const boost::system::error_code& error, std::size_t length)
            {
                if (!error)
                {
                    std::cout << "Reply: ";
                    std::cout.write(reply_, length);
                    std::cout << "\n";
                }
                else
                {
                    std::cout << "Read failed: " << error.message() << "\n";
                }
            });
    }

    boost::asio::ssl::stream<ba::ip::tcp::socket> socket_;
    char request_[MAX_LENGTH];
    char reply_[MAX_LENGTH];
};

int main(int argc, char* argv[])
{
    ba::io_context io_context;

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

    auto socket = std::make_shared< ba::ip::udp::socket>(io_context);
    std::cout << sizeof(socket) << std::endl;
    socket->open(ba::ip::udp::v4(), error);
    if (error) {
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

    

    ba::ip::tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve("playground.tesonet.lt", "https");

    boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
    //ctx.load_verify_file("ca.pem");

    client c(io_context, ctx, endpoints);

    io_context.run();

    return 0;
}
