#pragma once
#ifndef liuzianglib_server_base
#define liuzianglib_server_base
#include <boost\asio.hpp>
#include <boost\asio\steady_timer.hpp>
#include <thread>
#include <mutex>
#include "DC_Test.h"
//Version 2.4.21V35
//20171022

namespace DC {

	namespace Web {

		namespace Server {

			namespace serverSpace {

				using tcp_socket = boost::asio::ip::tcp::socket;

				class buffer : public std::istream {
				public:
					buffer() :std::istream(&stream_buffer) {}

				public:
					std::size_t size() noexcept {
						return stream_buffer.size();
					}

					std::string string() noexcept {
						try {
							std::stringstream ss;
							ss << rdbuf();
							return ss.str();
						}
						catch (...) {
							return std::string();
						}
					}

				public:
					boost::asio::streambuf stream_buffer;
				};

				template<typename socket_type>
				class connection_base :public std::enable_shared_from_this<connection_base<socket_type>> {
				public:
					template <typename ...ARGS>
					connection_base(boost::asio::io_service& io_service, ARGS&&... args) :timer(io_service), socket(new socket_type(std::forward<ARGS>(args)...)) {}

					virtual ~connection_base() {
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

						std::unique_lock<std::mutex> lock(mutex);
						boost::system::error_code ec;
						socket->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
						socket->lowest_layer().close(ec);
					}

				public:
					boost::asio::steady_timer timer;
					std::unique_ptr<socket_type> socket;//用unique_ptr是因为asio::ssl::stream<asio::ip::tcp::socket>不可移动也不可拷贝
					std::mutex mutex;
					buffer buffer;

					DC::Test test;
				};

				struct server_config_base {
					std::string address;
					unsigned short port;
					std::size_t thread_number = 1;
					std::size_t max_request_size = std::numeric_limits<std::size_t>::max();
					std::size_t max_keep_alive = 5;
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

						post_accept();

						threads.clear();
						for (std::size_t i = 1; i < config->thread_number; i++)
							threads.emplace_back([this] {this->io_service.run(); });

						if (config->thread_number == 1)
							io_service.run();

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

					server_config_base get_config()const {
						if (!config)
							throw std::exception();

						return *config;
					}

				protected:
					virtual void post_accept() = 0;

				public:
					boost::asio::io_service io_service;

				protected:
					boost::asio::ip::tcp::acceptor acceptor;
					std::unique_ptr<server_config_base> config;
					std::vector<std::thread> threads;
				};

			}

		}

	}

}
#endif
