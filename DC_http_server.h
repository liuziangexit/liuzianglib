#pragma once
#ifndef liuzianglib_http_server
#define liuzianglib_http_server
#include "DC_server_base.h"
#include "DC_http.h"
#include <atomic>
//Version 2.4.21V35
//20171022

namespace DC {

	namespace Web {

		namespace Server {

			namespace ServerSpace {

				DC::Web::http::response make_response(const DC::Web::http::status_code& status_code) {
					DC::Web::http::response response;
					response.set_status_code(status_code);
					response.set_version(1.1);
					return response;
				}

			}

			struct http_server_config :public ServerSpace::server_config_base {
				http_server_config() {
					port = 80;
				}
			};

			using http_connection = ServerSpace::connection_base<ServerSpace::tcp_socket>;
			/*
			class http_connection :public ServerSpace::connection_base<ServerSpace::tcp_socket> {
			public:
				template <typename ...ARGS>
				http_connection(ARGS&&... args) : ServerSpace::connection_base<ServerSpace::tcp_socket>(std::forward<ARGS>(args)...), close_flag(false) {};
				
			public:
				virtual void close() {
					close_flag.store(true);
					ServerSpace::connection_base<ServerSpace::tcp_socket>::close();
				}

			public:
				std::atomic<bool> close_flag;
			};
			*/

			class http_server :public ServerSpace::server_base<ServerSpace::tcp_socket> {
			public:
				http_server() {}

			protected:
				virtual void post_accept()override {
					auto connection = std::make_shared<http_connection>(this->io_service, this->get_config().max_request_size, this->io_service);

					acceptor.async_accept(*connection->socket, [this, connection](const boost::system::error_code& ec) {
						//投递一个新的accept
						if (ec != boost::asio::error::operation_aborted)
							this->post_accept();

						if (ec)
							return;

						boost::asio::ip::tcp::no_delay option(this->get_config().tcp_nodelay);
						boost::system::error_code ec2;
						connection->socket->set_option(option, ec2);						

						this->post_recv(connection);
					});
				}

				void post_recv(std::shared_ptr<http_connection> connection) {
					//auto lock = std::make_shared<std::unique_lock<std::mutex>>(connection->mutex);
					//if (connection->close_flag.load())
					//	return;
					connection->set_timeout(std::chrono::seconds(this->get_config().max_request_time));
					std::function<void(void)> post_recv(std::bind(&http_server::post_recv, this, connection));
					connection->buffer.clear();
					std::function<void(const std::string&)> reply(std::bind(&http_server::post_send, this, connection, std::placeholders::_1));
					boost::asio::async_read_until(*connection->socket, connection->buffer.stream_buffer, "\r\n\r\n", [this, connection, reply,post_recv](const boost::system::error_code& ec, const std::size_t& bytes_transfered)mutable {
						//lock.reset();
						connection->cancel_timeout();

						if ((!ec || ec == boost::asio::error::not_found) && connection->buffer.stream_buffer.size() == connection->buffer.stream_buffer.max_size()) {
							auto response = ServerSpace::make_response(DC::Web::http::status_codes::BadRequest);
							reply(response.toStr());
							return;
						}

						if (!ec) {
							DC::Web::http::request request;
							try {
								request = DC::Web::http::request_deserialization(connection->buffer.string());
							}
							catch (...) {
								auto response = ServerSpace::make_response(DC::Web::http::status_codes::BadRequest);
								reply(response.toStr());
								return;
							}

							DC::Web::http::response response;
							try {
								response = on_recv(request, connection->socket->remote_endpoint().address().to_string());
							}
							catch (...) {
								response = ServerSpace::make_response(DC::Web::http::status_codes::InternalError);
								reply(response.toStr());
								return;
							}

							reply(response.toStr());
						}
					});
				}

				void post_send(std::shared_ptr<http_connection> connection, const std::string& send_str) {
					//auto lock = std::make_shared<std::unique_lock<std::mutex>>(connection->mutex);
					//if (connection->close_flag.load())
					//	return;

					std::function<void(void)> post_recv(std::bind(&http_server::post_recv, this, connection));
					boost::asio::async_write(*connection->socket, boost::asio::buffer(send_str), [send_str, this, connection, post_recv](const boost::system::error_code &ec, std::size_t)mutable {
						//lock.reset();
						//this->on_send(send_str, connection->socket->remote_endpoint().address().to_string());

						//if (connection->close_flag.load())
						//	return;

						//post_recv();
					});
				}

			public:				
				std::function<DC::Web::http::response(const DC::Web::http::request&, const std::string&)> on_recv;
				std::function<void(const std::string&, const std::string&)> on_send;
			};

		}

	}

}
#endif
