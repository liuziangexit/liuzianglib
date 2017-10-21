#pragma once
#ifndef liuzianglib_http_server
#define liuzianglib_http_server
#include "DC_server_base.h"
#include "DC_http.h"
//Version 2.4.21V35
//20171022

namespace DC {

	namespace Web {

		namespace Server {

			struct http_server_config :public serverSpace::server_config_base {
				http_server_config() {
					port = 80;
				}
			};

			class http_connection :public serverSpace::connection_base<serverSpace::tcp_socket> {
			public:
				template <typename ...ARGS>
				http_connection(ARGS&&... args) : serverSpace::connection_base<serverSpace::tcp_socket>(std::forward<ARGS>(args)...) {};

			public:
				DC::Web::http::request request;
			};

			class http_server :public serverSpace::server_base<serverSpace::tcp_socket> {
			public:
				http_server() {}

			protected:
				virtual void post_accept()override {
					auto connection = std::make_shared<http_connection>(this->io_service, this->io_service);

					acceptor.async_accept(*connection->socket, [this, connection](const boost::system::error_code& ec) {
						//投递一个新的accept
						if (ec != boost::asio::error::operation_aborted)
							this->post_accept();

						if (ec) {
							if (on_error)
								on_error(std::exception("on_accept bad code"));
							return;
						}

						boost::asio::ip::tcp::no_delay option(this->get_config().tcp_nodelay);
						boost::system::error_code ec2;
						connection->socket->set_option(option, ec2);
						connection->set_timeout(std::chrono::seconds(this->get_config().max_keep_alive));

						this->post_recv(connection);
					});
				}

				void post_recv(std::shared_ptr<http_connection> connection) {
					auto lock = std::make_shared<std::unique_lock<std::mutex>>(connection->mutex);

					boost::asio::async_read_until(*connection->socket, connection->buffer.stream_buffer, "\r\n\r\n", std::bind([connection](const boost::system::error_code& ec, const std::size_t& bytes_transfered, std::shared_ptr<std::unique_lock<std::mutex>> lock) {
						std::cout << connection->buffer.string();
					}, std::placeholders::_1, std::placeholders::_2, lock));
				}

			public:
				std::function<void(const std::exception&)> on_error;
			};

		}

	}

}
#endif
