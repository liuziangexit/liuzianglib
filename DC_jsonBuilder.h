#pragma once
#ifndef liuzianglib_jsonBuidler
#define liuzianglib_jsonBuilder
#include <vector>
#include <string>
#include "liuzianglib.h"
#include "DC_STR.h"
//Version 2.4.2V45
//20170604

namespace DC {

	namespace Web {

		namespace jsonBuilder {

			namespace jsonBuilderSpace {
			
				enum DataType {
					string_t,
					int_t,
					double_t,
					null_t,
					bool_t,
					empty_t
				};

				class NV_base {//number和value的基类
				public:
					virtual ~NV_base() = default;

				public:
					inline void fromNull(void) {
						rawStr = "null";
						type = jsonBuilderSpace::DataType::null_t;
					}

					inline jsonBuilderSpace::DataType getType()const noexcept {
						return this->type;
					}

					inline std::string getValue()const {
						switch (type) {
						case jsonBuilderSpace::DataType::bool_t: {
							if (rawStr == "1") return "true";
							return "false";
						}break;
						case jsonBuilderSpace::DataType::double_t: {
							return rawStr;
						}break;
						case jsonBuilderSpace::DataType::empty_t: {
							throw DC::Exception("jsonBuilder::jsonBuilderSpace::NV_base", "empty value");
						}break;
						case jsonBuilderSpace::DataType::int_t: {
							return rawStr;
						}break;
						case jsonBuilderSpace::DataType::null_t: {
							return rawStr;
						}break;
						case jsonBuilderSpace::DataType::string_t: {
							return '"' + rawStr + '"';
						}break;
						}
					}

					inline void clear() {
						rawStr.clear();
						type = DataType::empty_t;
					}

				protected:
					std::string rawStr;
					DataType type;
				};

				class JSKeyValuePair final :public DC::KeyValuePair {
				public:
					JSKeyValuePair(const JSKeyValuePair&) = default;

					JSKeyValuePair(JSKeyValuePair&& input) {
						name = std::move(input.name);
						value = std::move(input.value);
						OK = input.OK;
						input.OK = false;
					}

				public:
					template <typename T>
					inline void SetName(T&& input) {
						this->name = std::forward<T>(input);
					}

					template <typename T>
					inline void SetValue(T&& input) {
						this->value = std::forward<T>(input);
					}

					virtual inline char GetSeparator()const noexcept { return 0; }
				};

			}

			class value final :public jsonBuilderSpace::NV_base {
			public:
				value() {
					type = jsonBuilderSpace::DataType::empty_t;
				}

				value(const value&) = default;

				value(value&& input) {
					this->rawStr = std::move(input.rawStr);
					this->type = input.type;
					input.type = jsonBuilderSpace::DataType::empty_t;
				}

			public:
				template <typename T>
				inline void fromString(T&& input) {
					static_assert(std::is_same<std::string, typename std::decay<T>::type>::value, "input type should be std::string");
					rawStr = std::forward<T>(input);
					type = jsonBuilderSpace::DataType::string_t;
				}

				inline void fromBool(const bool& input) {
					rawStr = DC::STR::toString(input);
					type = jsonBuilderSpace::DataType::bool_t;
				}
			};

			class number final :public jsonBuilderSpace::NV_base {
			public:
				number() {
					type = jsonBuilderSpace::DataType::empty_t;
				}

				number(const number&) = default;

				number(number&& input) {
					this->rawStr = std::move(input.rawStr);
					this->type = input.type;
					input.type = jsonBuilderSpace::DataType::empty_t;
				}

			public:
				inline void fromInt32(const int32_t& input) {
					rawStr = DC::STR::toString(input);
					type = jsonBuilderSpace::DataType::int_t;
				}

				inline void fromDouble(const double& input) {
					rawStr = DC::STR::toString(input);
					type = jsonBuilderSpace::DataType::double_t;
				}
			};

			class object final {
			public:
				object() = default;

				object(const object&) = default;

				object(object&&) = default;

			public:
				std::string toString()const {}

				template <typename ...ARGS>
				inline void add(ARGS&& ...args) {
					m_data.emplace_back(std::forward<ARGS>(args)...);
				}

				inline void add(const jsonBuilderSpace::JSKeyValuePair& input) {
					m_data.push_back(input);
				}

				inline void add(jsonBuilderSpace::JSKeyValuePair&& input) {
					m_data.push_back(std::move(input));
				}

				inline void clear() {
					m_data.clear();
				}

			private:
				std::vector<jsonBuilderSpace::JSKeyValuePair> m_data;
			};

		}

	}

}

#endif