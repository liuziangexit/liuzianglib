#pragma once
#ifndef liuzianglib_http
#define liuzianglib_http
#include "liuzianglib.h"
#include "DC_STR.h"
#include <vector>
#include <string>
#include <cctype>
//Version 2.4.2V10
//20170414

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

			title(const std::string& inputVersion, const std::string& inputOther) :version(inputVersion), other(inputOther) {}

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
			template<typename ...argsType, class = typename std::enable_if_t<!std::is_same<std::decay_t<argsType...>, std::string>::value>>
			void add(argsType&& ...args) {//用法:add(addHeader("name", "value"), addHeader("name2", "value2"), addHeader("name3", "value3")......);
				auto args_value = DC::GetArgs(std::forward<argsType>(args)...);
				for (const auto& p : args_value) {
					m_data.push_back(std::move(p.get<httpSpace::header>()));
				}
			}

			inline void add(const std::string& input) {
				m_data.emplace_back(input);
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

			static const char *emptyline = "\r\n\r\n";

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
			inline void Derived_construct(base& object, titleType&& inputtitle, headersType&& inputheaders, bodyType&& inputbody) {
				static_assert(std::is_same<std::decay_t<titleType>, title>::value, "first arg's type should be title");
				static_assert(std::is_same<std::decay_t<headersType>, headers>::value, "second arg's type should be headers");
				static_assert(std::is_same<std::decay_t<bodyType>, body>::value, "third arg's type should be body");
				object.setTitle(std::forward<title>(inputtitle));
				object.setHeaders(std::forward<headers>(inputheaders));
				object.setBody(std::forward<body>(inputbody));
			}

			template<typename T>
			http::title title_deserialization(const std::string& input);

			template<>
			inline http::title title_deserialization<http::request>(const std::string& input) {
				auto loca = DC::STR::find(input, " ");
				if (loca.getplace_ref().empty()) throw DC::DC_ERROR("title_deserialization", "method not found", 0);
				auto GetVersionNumStr = [](const std::string& input) {
					std::string rv;
					for (const auto& p : input) {
						if (std::isdigit(p) || p == '.') rv.push_back(p);
					}
					return rv;
				};
				return http::title(GetVersionNumStr(DC::STR::getSub(input, *loca.getplace_ref().rbegin(), input.size())), DC::STR::getSub(input, -1, *loca.getplace_ref().rbegin()));
			}

			template<>
			inline http::title title_deserialization<http::response>(const std::string& input) {
				auto loca = DC::STR::find(input, " ");
				if (loca.getplace_ref().empty()) throw DC::DC_ERROR("title_deserialization", "method not found", 0);
				auto GetVersionNumStr = [](const std::string& input) {
					std::string rv;
					for (const auto& p : input) {
						if (std::isdigit(p) || p == '.') rv.push_back(p);
					}
					return rv;
				};
				return http::title(GetVersionNumStr(DC::STR::getSub(input, -1, *loca.getplace_ref().begin())), DC::STR::getSub(input, *loca.getplace_ref().rbegin(), input.size()));
			}

			http::headers headers_deserialization(const std::string& input) {
				http::headers rv;
				std::string temp;
				for (const auto& p : input) {
					if (p == '\r') { rv.add(temp); temp.clear(); continue; }
					temp.push_back(p);
				}
				if (!temp.empty()) rv.add(temp);
				return rv;
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

		request request_deserialization(const std::string& input) {
			std::string titleraw, headersraw, bodyraw;
			std::size_t titleend = 0, headersend = 0;

			for (std::size_t i = 0; i < input.size(); i++) {
				if (input[i] == '\n') { titleend = i; break; }
				titleraw.push_back(input[i]);
			}

			auto emptylineLoca = DC::STR::find(input, httpSpace::emptyline);
			if (emptylineLoca.getplace_ref().empty()) throw DC::DC_ERROR("request_deserialization", "emptyline not found", 0);
			headersraw = DC::STR::getSub(input, titleend, *emptylineLoca.getplace_ref().begin());

			bodyraw = DC::STR::getSub(input, *emptylineLoca.getplace_ref().begin() + sizeof(httpSpace::emptyline), input.size());

			return request(title(httpSpace::title_deserialization<request>(titleraw)), httpSpace::headers_deserialization(headersraw), bodyraw);
		}

		response response_deserialization(const std::string& input) {
			std::string titleraw, headersraw, bodyraw;
			std::size_t titleend = 0, headersend = 0;

			for (std::size_t i = 0; i < input.size(); i++) {
				if (input[i] == '\n') { titleend = i; break; }
				titleraw.push_back(input[i]);
			}

			auto emptylineLoca = DC::STR::find(input, httpSpace::emptyline);
			if (emptylineLoca.getplace_ref().empty()) throw DC::DC_ERROR("response_deserialization", "emptyline not found", 0);
			headersraw = DC::STR::getSub(input, titleend, *emptylineLoca.getplace_ref().begin());

			bodyraw = DC::STR::getSub(input, *emptylineLoca.getplace_ref().begin() + sizeof(httpSpace::emptyline), input.size());

			return response(title(httpSpace::title_deserialization<response>(titleraw)), httpSpace::headers_deserialization(headersraw), bodyraw);
		}

	}

}

#endif