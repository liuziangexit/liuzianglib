#pragma once
#ifndef liuzianglib_http
#define liuzianglib_http
#include "liuzianglib.h"
#include "DC_STR.h"
#include <vector>
#include <string>
//Version 2.4.2V9
//20170413

namespace DC {

	namespace http {

		namespace httpSpace {

			class header final :public DC::KeyValuePair {
			public:
				header() = default;

				header(const std::string& input) :KeyValuePair() {
					Set(input);
				}

				header(const std::string& key, const std::string& inputvalue) :KeyValuePair() {
					name = key;
					value = inputvalue;
					OK = true;
				}

			public:
				virtual inline char GetSeparator()const noexcept {
					return ':';
				}

			protected:
				virtual inline bool isSeparator(const char& ch)const noexcept override {
					if (ch == ':') return true;
					return false;
				}
			};

		}

		class request;
		class response;
		class headers;

		using body = std::string;
		
		class title final {
		public:
			title() = default;

			title(const double& inputVersion, const std::string& inputOther) :version(DC::STR::toString(inputVersion)), other(inputOther) {}

		public:
			template<typename T>
			inline std::string toStr()const;

			template<>
			inline std::string toStr<DC::http::request>()const {
				return other + " " + GetVersionStr();
			}

			template<>
			inline std::string toStr<DC::http::response>()const {
				return GetVersionStr() + " " + other;
			}

		public:
			inline void setVersion(const double& num) {
				version = DC::STR::toString(num);
			}

			inline void setOther(const std::string& input) {
				other = input;
			}

		private:
			std::string version, other;

		private:
			inline std::string GetVersionStr()const noexcept {
				return "HTTP/" + version;
			}
		};

		class headers final {
		public:
			headers() = default;

			template<typename T, typename ...argsType>
			headers(T&& first, argsType&& ...args) {//用法:http_header(addHeader("name", "value"), addHeader("name2", "value2"), addHeader("name3", "value3")......);
				auto args_value = DC::GetArgs(std::forward<argsType>(args)...);
				m_data.push_back(std::move(first));
				for (const auto& p : args_value) {
					m_data.push_back(std::move(p.get<httpSpace::header>()));
				}
			}

			headers(const headers&) = default;

			headers(headers&& input) :m_data(std::move(input.m_data)) {}

		public:
			headers& operator=(const headers&) = default;
						
			headers& operator=(headers&& input) {
				m_data = std::move(input.m_data);
				return *this;
			}

		public:
			template<typename ...argsType>
			void add(argsType&& ...args) {//用法:add(addHeader("name", "value"), addHeader("name2", "value2"), addHeader("name3", "value3")......);
				auto args_value = DC::GetArgs(std::forward<argsType>(args)...);
				for (const auto& p : args_value) {
					m_data.push_back(std::move(p.get<httpSpace::header>()));
				}
			}

			inline void clear() {
				m_data.clear();
			}

			inline bool empty()const {
				return m_data.empty();
			}

			std::string toStr()const {
				std::string returnvalue;
				for (const auto& p : m_data) {
					returnvalue += p.GetName() += p.GetSeparator() + p.GetValue() + '\n';
				}
				returnvalue.erase(--returnvalue.rbegin().base());
				return returnvalue;
			}

		public:
			inline const std::vector<httpSpace::header>& get()const {
				return m_data;
			}

		private:
			std::vector<httpSpace::header> m_data;
		};

		namespace httpSpace {

			static const char *emptyline = "\n\n";

			class base {
			public:
				base() = default;

				base(const base&) = default;

				base(base&& input) :m_title(std::move(input.m_title)), m_headers(std::move(input.m_headers)), m_body(std::move(input.m_body)) {}

				virtual ~base()noexcept = default;

			public:
				base& operator=(const base&) = default;

				base& operator=(base&& input) {
					m_title = std::move(input.m_title);
					m_headers = std::move(input.m_headers);
					m_body = std::move(input.m_body);
				}

			public:
				inline void setTitle(const title& input) {
					m_title = input;
				}

				template<typename T>
				inline void setHeaders(T&& input) {
					static_assert(std::is_same<std::decay_t<T>, headers>::value, "input type should be headers");
					m_headers = std::forward<T>(input);
				}
				
				template<typename T>
				inline void setBody(T&& input) {
					m_body = std::forward<T>(input);
				}

				virtual std::string toStr()const = 0;

			protected:
				DC::http::title m_title;
				headers m_headers;
				body m_body;
			};

			template<typename titleType, typename headersType, typename bodyType>
			void Derived_construct(base& object, titleType&& inputtitle, headersType&& inputheaders, bodyType&& inputbody) {
				static_assert(std::is_same<std::decay_t<titleType>, title>::value, "first arg's type should be title");
				static_assert(std::is_same<std::decay_t<headersType>, headers>::value, "second arg's type should be headers");
				static_assert(std::is_same<std::decay_t<bodyType>, body>::value, "third arg's type should be body");
				object.setTitle(std::forward<title>(inputtitle));
				object.setHeaders(std::forward<headers>(inputheaders));
				object.setBody(std::forward<body>(inputbody));
			}

		}
		
		class request :public httpSpace::base {
		public:
			request() = default;

			template<typename ...argsType>
			request(argsType ...args) {
				httpSpace::Derived_construct(reinterpret_cast<httpSpace::base&>(*this), std::forward<argsType>(args)...);
			}

		public:
			virtual inline std::string toStr()const override {
				return m_title.toStr<request>() + '\n' + m_headers.toStr() + httpSpace::emptyline + m_body;
			}
		};

		class response :httpSpace::base {
		public:
			response() = default;

			template<typename ...argsType>
			response(argsType ...args) {
				httpSpace::Derived_construct(reinterpret_cast<httpSpace::base&>(*this), std::forward<argsType>(args)...);
			}

		public:
			virtual inline std::string toStr()const override {
				return m_title.toStr<response>() + '\n' + m_headers.toStr() + httpSpace::emptyline + m_body;
			}
		};
		
		template<typename ...argsType>
		httpSpace::header addHeader(argsType&& ...args) {
			return httpSpace::header(std::forward<argsType>(args)...);
		}

		request request_deserialization() {
			return request();
		}

		response response_deserialization() {
			return response();
		}

	}

}

#endif