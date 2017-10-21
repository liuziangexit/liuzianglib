#pragma once
#ifndef liuzianglib_http_server
#define liuzianglib_http_server
#include "server_base.h"
#include "DC_http.h"
//Version 2.4.21V34
//20171021

namespace DC {

	namespace Web {

		namespace Server {

			struct http_server_config :public serverSpace::server_base_config {
				http_server_config() {
					port = 80;
				}
			};

			class http_connection :public serverSpace::connection<serverSpace::tcp_socket> {
			public:
				template <typename ...ARGS>
				http_connection(ARGS&&... args) : serverSpace::connection<serverSpace::tcp_socket>(std::forward<ARGS>(args)...) {};

			public:
				DC::Web::http::request request;
			};

			class http_server :public serverSpace::server_base<serverSpace::tcp_socket> {
			public:
				http_server() {}

			protected:
				virtual void accept()override {
					auto connection = std::make_shared<http_connection>(this->io_service, this->io_service);

					acceptor.async_accept(*connection->socket, [this, connection](const boost::system::error_code& ec) {
						//投递一个新的accept
						if (ec != boost::asio::error::operation_aborted)
							this->accept();

						if (ec) {
							//on_error
							return;
						}

						boost::asio::ip::tcp::no_delay option(this->get_config().tcp_nodelay);
						boost::system::error_code ec2;
						connection->socket->set_option(option, ec2);

					});
				}
			};

		}

	}

}
#endif
