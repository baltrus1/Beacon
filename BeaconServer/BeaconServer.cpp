// BeaconServer.cpp : Defines the entry point for the application.
//

#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind/bind.hpp>

#include <queue>
#include <memory>

#define BROADCAST_PORT 1234
#define P2P_PORT 4321
#define MAX_LENGTH 3024


namespace ba = boost::asio;

static boost::asio::io_context io_context;

// Thread safe logging. Once client and server is one binary extract this to reuse in both
struct Logger {
	void log(const std::string& tag, const std::string& message) {
		std::lock_guard<std::mutex> guard(m_stdoutMutex);
		std::cout << tag << ": " << message;
		std::cout.flush();
	}
private:
	std::mutex m_stdoutMutex;
};

static Logger s_logger;

class PeriodicBroadcasting {
	int m_interval;
	std::atomic<bool> m_running = false;
	std::unique_ptr<std::thread> m_thread;
	boost::system::error_code m_error;
public:
	PeriodicBroadcasting(int interval = 1)
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
					s_logger.log("SERVER", "Broadcast send failed: " + m_error.message() + "\n");
				}
				std::this_thread::sleep_for(std::chrono::seconds(m_interval));
			}
			catch (...) {}
		}
	}

};


// modified version of https://www.boost.org/doc/libs/1_73_0/doc/html/boost_asio/example/cpp11/ssl/client.cpp
class Proxy {
public:
	explicit Proxy(ba::ip::udp::endpoint& clientEndpoint)
		: ctx(boost::asio::ssl::context::sslv23_client)
		, socket_(io_context, ctx)
		, m_clientEndpoint(clientEndpoint)
	{
		ba::ip::tcp::resolver resolver(io_context);
		endpoints_ = resolver.resolve("playground.tesonet.lt", "https");
		socket_.set_verify_mode(boost::asio::ssl::verify_none);

		// From here everything is done async
		connect();
	}

	void connect() {
		ba::async_connect(socket_.lowest_layer(), endpoints_,
			[this](const boost::system::error_code& error,
				const ba::ip::tcp::endpoint& /*endpoint*/) {
				if (!error) {
					handshake();
				} else {
					s_logger.log("SERVER", "Connect failed: " + error.message() + "\n");
				}
			});
	}

private:
	void handshake() {
		socket_.async_handshake(boost::asio::ssl::stream_base::client,
			[this](const boost::system::error_code& error) {
				if (!error) {
					send_request();
				} else {
					s_logger.log("SERVER", "Handshake failed: " + error.message() + "\n");
				}
			});
	}

	void send_request()
	{
		std::ostringstream request_stream;
		//std::string json = "{\"username\":\"tesonet\",\"password\":\"partyanimal\"}";

		request_stream << "GET /v1/servers HTTP/1.1 \r\n";
		request_stream << "Host: " << "playground.tesonet.lt" << "\r\n";
		request_stream << "User-Agent: curl/7.55.1" << "\r\n";
		request_stream << "Accept: */*\r\n";
		request_stream << "Connection: close\r\n";
		request_stream << "Authorization: Bearer f9731b590611a5a9377fbd02f247fcdf\r\n\r\n";


		/*request_stream << "Content-Type: application/json\r\n";
		request_stream << "Content - Length: " << json.length() << "\r\n\r\n";
		request_stream << json << "\r\n";*/


		auto request_str = request_stream.str();
		//std::cout << "Request: " << std::endl << std::endl << request_str << std::endl;

		ba::async_write(socket_,
			boost::asio::buffer(request_str, request_str.length()),
			[this](const boost::system::error_code& error, std::size_t length) {
				if (!error) {
					receive_response();
				} else {
					s_logger.log("SERVER", "Write failed: " + error.message() + "\n");
				}
			});
	}

	void receive_response()
	{
		boost::asio::async_read(socket_,
			boost::asio::buffer(reply_),
			[this](const boost::system::error_code& error, std::size_t length) {
				// Error will occur as soon as server has no more data and closes connection and then this will execute
				resendToClient(length);
			});
	}

	void resendToClient(int length) {
		std::string response(reply_, length);
		size_t startPos = response.find("[{\"");
		size_t endPos = response.find("}]");
		std::string body = response.substr(startPos, endPos - startPos + 2);

		std::ostringstream oss;
		oss << "Sending data to client:\n" << body << "\n\n";
		s_logger.log("SERVER", oss.str());

		boost::system::error_code ec;
		ba::ip::udp::socket sock(io_context);

		sock.open(ba::ip::udp::v4(), ec);
		if (ec) {
			s_logger.log("SERVER", "Socket open failed: " + ec.message() + "\n");
			return;
		}

		sock.send_to(boost::asio::buffer(body, body.length()), m_clientEndpoint, 0, ec);
		if (ec) {
			s_logger.log("SERVER", "Failed to send server list to client: " + ec.message() + "\n");
			return;
		}
	}

	ba::ssl::context ctx;
	ba::ip::tcp::resolver::results_type endpoints_;
	ba::ssl::stream<ba::ip::tcp::socket> socket_;
	ba::ip::udp::endpoint m_clientEndpoint;
	char reply_[MAX_LENGTH];
};

// modified version of https://www.boost.org/doc/libs/1_73_0/doc/html/boost_asio/tutorial/tutdaytime6/src.html
// This udp_server is responsible for catching client requests, creating Proxy object for each 
// such client and returning data from Proxy object to that client.
class udp_server
{
public:
	udp_server()
		: socket_(io_context, ba::ip::udp::endpoint(ba::ip::udp::v4(), P2P_PORT))
	{
		start_receive();
	}

private:
	void start_receive()
	{
		socket_.async_receive_from(
			boost::asio::buffer(recv_buffer_), source_endpoint,
			boost::bind(&udp_server::handle_receive, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
	}

	void handle_receive(const boost::system::error_code& ec,
		std::size_t /*bytes_transferred*/)
	{
		if (ec) {
			s_logger.log("SERVER", ec.message() + "\n");
			return;
		}

		// async proxy creation and data transmission
		proxies.push(std::make_shared<Proxy>(source_endpoint));

		start_receive();
	}

	// Implement proxy deletion once it finishes serving the client(change structure)
	std::queue<std::shared_ptr<Proxy>> proxies;
	ba::ip::udp::socket socket_;
	ba::ip::udp::endpoint source_endpoint;
	char recv_buffer_[MAX_LENGTH];
};

int main(int argc, char* argv[]) {
	boost::system::error_code ec;

	// should move these to pb class
	auto socket = std::make_shared< ba::ip::udp::socket>(io_context);
	socket->open(ba::ip::udp::v4(), ec);
	if (ec) {
		std::cerr << "SERVER: Socket open failed: " << ec.message() << std::endl;
	}

	ba::socket_base::broadcast option(true);
	socket->set_option(option);

	ba::ip::udp::endpoint remote_endpoint = ba::ip::udp::endpoint(ba::ip::address_v4::broadcast(), BROADCAST_PORT);

	const char* message = "I am still here";
	int len = strlen(message);
	ba::const_buffer buffer = ba::buffer(message, len + 1);

	PeriodicBroadcasting pb(1);

	pb.start(socket, buffer, remote_endpoint);
	udp_server server;
	std::cout << "SERVER: running.." << std::endl;

	io_context.run();
	std::this_thread::sleep_for(std::chrono::seconds(200));
	return 0;
}
