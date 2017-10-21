#pragma once
#ifndef liuzianglib_server_base
#define liuzianglib_server_base
#include <boost\asio.hpp>
#include <boost\asio\steady_timer.hpp>
#include <thread>
//Version 2.4.21V34
//20171021

namespace DC {

	namespace Web {

		namespace Server {

			namespace serverSpace {

				using tcp_socket = boost::asio::ip::tcp::socket;

				template<typename socket_type>
				class connection :public std::enable_shared_from_this<connection<socket_type>> {
				public:
					template <typename ...ARGS>
					connection(boost::asio::io_service& io_service, ARGS&&... args) :timer(io_service), socket(new socket_type(std::forward<ARGS>(args)...)) {}

					connection(const connection&) = delete;

					connection(connection&&) = default;

					connection& operator=(const connection&) = delete;

					connection& operator=(connection&&) = default;

					virtual ~connection() {
						this->close();
					}

				public:
					void set_timeout(const std::chrono::seconds& timeout) {
						if (timeout.count() == 0)
							return;

						auto self = this->shared_from_this();
						timer.expires_from_now(timeout);
						timer.async_wait([self](const boost::system::error_code& ec) {
							if (!ec)
								self->close();
						});
					}

					inline void close() {
						if (!socket)
							return;
						socket->close();
					}

				public:
					boost::asio::steady_timer timer;
					std::unique_ptr<socket_type> socket;//用unique_ptr是因为asio::ssl::stream<asio::ip::tcp::socket>不可移动也不可拷贝

					//for test
					//DC::Test
				};

				struct server_base_config {
					std::string address;
					unsigned short port;
					std::size_t thread_number = 1;
					std::size_t max_request_size = std::numeric_limits<std::size_t>::max();
					bool tcp_nodelay = false;
				};

				template <typename socket_type>
				class server_base {
				public:
					server_base() :acceptor(io_service) {}

					virtual ~server_base()noexcept {
						this->stop();
					}

				public:
					virtual void start() {
						if (!config)
							return;

						if (io_service.stopped())
							io_service.reset();

						boost::asio::ip::tcp::endpoint endpoint;
						if (!config->address.empty())
							endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(config->address), config->port);
						else
							endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), config->port);

						acceptor.open(endpoint.protocol());
						acceptor.bind(endpoint);
						acceptor.listen();

						accept();

						threads.clear();
						for (std::size_t i = 0; i < config->thread_number; i++)
							threads.emplace_back([this] {this->io_service.run(); });

						for (auto& p : threads)
							p.join();
					}

					void stop() {
						boost::system::error_code ec;
						acceptor.close(ec);

						io_service.stop();
					}

					template <typename config_type>
					inline void set_config(config_type& input_config) {
						config.reset(new config_type(input_config));
					}

					server_base_config get_config()const {
						if (!config)
							throw std::exception();

						return *config;
					}

				protected:
					virtual void accept() = 0;

				public:
					boost::asio::io_service io_service;

				protected:
					boost::asio::ip::tcp::acceptor acceptor;
					std::unique_ptr<server_base_config> config;
					std::vector<std::thread> threads;
				};

			}

		}

	}

}
#endif
