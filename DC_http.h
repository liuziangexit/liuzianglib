#pragma once
#ifndef liuzianglib_http
#define liuzianglib_http
#include "liuzianglib.h"
#include "DC_STR.h"
#include <vector>
#include <string>
#include <cctype>
//Version 2.4.2V12
//20170415

namespace DC {

	namespace http {

		class request;
		class response;
		class headers;

		using body = std::string;
		using method = const std::string;
		using status_code = const unsigned short;

		namespace methods {

			static http::method GET = "GET";
			static http::method POST = "POST";
			static http::method HEAD = "HEAD";

		}

		namespace status_codes {

			static http::status_code OK = 200;
			static http::status_code BadRequest = 400;
			static http::status_code Forbidden = 403;
			static http::status_code NotFound = 404;
			static http::status_code MethodNotAllowed = 405;
			static http::status_code InternalError = 500;
			static http::status_code ServiceUnavailable = 503;

		}

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

			const char* getSC(const http::status_code& input) {
				if (input == 200) { return "200 OK"; }
				if (input == 400) { return "400 Bad Request"; }
				if (input == 403) { return "403 Forbidden"; }
				if (input == 404) { return "404 Not Found"; }
				if (input == 405) { return "405 Method Not Allowed"; }
				if (input == 500) { return "500 Internal Error"; }
				if (input == 503) { return "503 Service Unavailable"; }
				throw DC::DC_ERROR("getSC", "unknown status code", 0);
			}

		}

		class title final {
			friend class request;
			friend class response;
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

			inline void SetMethod(const http::method& input) {//把method字符串放入第一个空格之前（替换掉原有的第一个空格之前的字符串），如果没有第一个空格，则把method放入开头，然后加空格，然后加原来的other的内容
				std::size_t loca = 0;
				for (; loca < other.size(); loca++) {
					if (other[loca] == ' ') break;
				}
				if (loca == other.size()) { other = input + " " + other; return; }
				other = input + DC::STR::getSub(other, loca - 1, other.size());
			}

			inline method GetMethod()const {//返回other里第一个空格之前的子串，如果没有则抛异常
				std::string method_str;
				for (const auto& p : other) {
					if (p != ' ') { method_str.push_back(p); continue; }
					break;
				}
				if (method_str.empty()) throw DC::DC_ERROR("GetMethod", "can't get method", 0);
				return method_str;
			}

			inline void SetStatusCode(const http::status_code& input) {
				other = httpSpace::getSC(input);
			}

			inline http::status_code GetStatusCode()const {
				std::string rv;
				for (const auto& p : other) {
					if (std::isdigit(p)) rv.push_back(p);
				}
				if (rv.empty()) throw DC::DC_ERROR("GetStatusCode", "there's no number in string \"other\"", 0);
				return DC::STR::toType<int32_t>(rv);
			}

			inline void SetURI(const std::string& input) {//放到第一个空格之后，空格不够抛异常
				auto frs = DC::STR::find(other, " ");
				if (frs.getplace_ref().empty()) throw DC::DC_ERROR("SetURI", "space is not enough", 0);
				auto before = DC::STR::getSub(other, -1, *frs.getplace_ref().begin());
				other = before + " " + input;
			}

			inline std::string GetURI()const {//返回第一个空格到结尾之间的东西，如果空格不够抛异常。
				auto frs = DC::STR::find(other, " ");
				if (frs.getplace_ref().empty()) throw DC::DC_ERROR("GetURI", "space is not enough", 0);
				return DC::STR::getSub(other, *frs.getplace_ref().begin(), other.size());
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

				inline const http::title& Title()const {
					return m_title;
				}

				inline http::headers& Headers() {
					return m_headers;
				}

				inline const http::body& Body()const {
					return m_body;
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

		class request final :public httpSpace::base {
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

			inline void setMethod(const http::method& input) {
				m_title.SetMethod(input);
			}

			inline method Method() {
				return m_title.GetMethod();
			}

			inline void setURI(const std::string& input) {
				m_title.SetURI(input);
			}

			inline std::string URI() {
				return m_title.GetURI();
			}
		};

		class response final :public httpSpace::base {
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

			inline void setStatusCode(const http::status_code& input) {
				m_title.SetStatusCode(input);
			}

			inline http::status_code StatusCode() {
				return m_title.GetStatusCode();
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