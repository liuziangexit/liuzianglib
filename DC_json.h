#pragma once
#ifndef liuzianglib_json
#define liuzianglib_json
#include <tuple>
#include <vector>
#include <limits>
#include <type_traits>
#include "DC_STR.h"
//Version 2.4.1V12
//20170325

namespace DC {

	namespace json {

		namespace jsonSpace {

			typedef std::tuple<std::size_t, std::size_t> PosPair;
			typedef std::vector<std::tuple<jsonSpace::PosPair, jsonSpace::PosPair>> ObjTable;

			inline bool SybolValid(const std::vector<std::size_t>& AllStartSymbol, const std::vector<std::size_t>& EndStartSymbol) {//判断开始符号和结束符号数量是否一样
				return AllStartSymbol.size() == EndStartSymbol.size();
			}

			std::vector<PosPair> getSymbolPair(const std::string& str, const char* const StartSymbol, const char* const EndSymbol) {//找出配对的符号，比如可以解析((2+2)*(1+1)*6)这种东西。
																																	//返回值是std::vector<PosPair>，每一个PosPair都是一对符号的位置，PosPair<0>是开始符号的位置，PosPair<1>是结束符号的位置
				                                                                                                                    //不支持StartSymbol==EndSymbol的情况
				std::vector<std::size_t> AllStartSymbol = DC::STR::find(str, StartSymbol).getplace_ref(), AllEndSymbol = DC::STR::find(str, EndSymbol).getplace_ref();
				std::vector<PosPair> returnvalue;

				if (!SybolValid(AllStartSymbol, AllEndSymbol)) throw DC::DC_ERROR("invalid string", "symbols can not be paired", 0);//判断开始符号和结束符号数量是否一样				
				//这个算法核心在于“距离AllStartSymbol中的最后一个元素最近且在其后的AllEndSymbol元素必然可以与之配对”。
				for (auto i = AllStartSymbol.rbegin(); i != AllStartSymbol.rend(); i = AllStartSymbol.rbegin()) {
					std::size_t minimal = INT_MAX;//int类型最大值
					auto iter = AllEndSymbol.end();
					for (auto i2 = AllEndSymbol.begin(); i2 != AllEndSymbol.end(); i2++) {
						if ((!(*i2 < *i)) && (*i2 - *i) < minimal) { minimal = *i2 - *i; iter = i2; }//找出和当前开始符号最近的结束符号
					}
					if (iter == AllEndSymbol.end())
						throw DC::DC_ERROR("undefined behavior", 0);//理论上应该不会抛出。。。
					returnvalue.emplace_back(*i, *iter);
					AllStartSymbol.erase(--i.base());
					AllEndSymbol.erase(iter);
				}
				return returnvalue;
			}

			std::vector<PosPair> getStringSymbolPair(const std::string& str) {//找出配对的""
				                                                              //不能嵌套哦
				std::vector<std::size_t> AllSymbol = DC::STR::find(str, "\"").getplace_ref();
				std::vector<PosPair> returnvalue;

				if (AllSymbol.size() % 2 != 0) throw DC::DC_ERROR("invalid string", "symbols can not be paired", 0);
				while(!AllSymbol.empty()) {
					returnvalue.emplace_back(*AllSymbol.begin(), *(AllSymbol.begin() + 1));
					AllSymbol.erase(AllSymbol.begin());
					AllSymbol.erase(AllSymbol.begin());
				}
				return returnvalue;
			}

			inline std::string GetJsStr(const std::string& input) {
				return "\"" + input + "\"";
			}

			ObjTable GetObjTable(const std::string& rawStr) {
				//写到这里，思路:逐个字符遍历raw，碰到格式控制字符如换行tab之类的直接跳过
				return ObjTable();
			}

		}

		class value {
		public:
			value() = default;

			template<typename T>
			value(T&& input)noexcept {
				try {
					set(std::forward<T>(input));
				}
				catch (...) {
					//异常后处理已经在set里保证过了，这里不用做过多操作，就阻止异常扩散就行了
				}
			}

			virtual ~value() = default;

		public:
			template<typename T>
			inline void set(T&& input) {
				static_assert(std::is_same<std::string, std::decay<T>::type>::value, "input type should be std::string");
				rawStr = std::forward<T>(input);
				try {
					refresh();
				}
				catch (const DC::DC_ERROR& ex) {
					if (!rawStr.empty()) rawStr.clear();
					throw ex;
				}
			}

			value at(const std::string& input) {

			}

		protected:
			std::string rawStr;
			std::vector<jsonSpace::PosPair> ObjectSymbolPair, ArraySymbolPair, StringSymbolPair;
			json::jsonSpace::ObjTable ObjectTable;//当前层的对象列表。第一个是对象名，第二个是对象内容。

		private:
			void refresh() {
				try {
					ObjectSymbolPair = jsonSpace::getSymbolPair(rawStr, "{", "}");
					ArraySymbolPair = jsonSpace::getSymbolPair(rawStr, "[", "]");
					StringSymbolPair = jsonSpace::getSymbolPair(rawStr, "\"", "\"");
				}
				catch (const DC::DC_ERROR& ex) {
					if (!ObjectSymbolPair.empty()) ObjectSymbolPair.clear();
					if (!ArraySymbolPair.empty()) ArraySymbolPair.clear();
					if (!ObjectTable.empty()) ObjectTable.clear();
					throw ex;
				}
			}
		};

	}
}

#endif