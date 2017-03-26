#pragma once
#ifndef liuzianglib_STR
#define liuzianglib_STR
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <sstream>
#include "DC_ERROR.h"
//Version 2.4.1V14
//20170326

namespace DC {

	namespace STR {

		namespace STRSpace {

			std::vector<std::size_t> KMPSearch(const char *pat, const char *txt) {
				std::vector<std::size_t> returnvalue;
				std::size_t M = strlen(pat), N = strlen(txt), j = 0, i = 0;
				std::unique_ptr<std::size_t[]> lps(new(std::nothrow) std::size_t[M]);
				if (lps.get() == nullptr) return returnvalue;
				auto computeLPSArray = [&pat, &M, &lps] {
					std::size_t len = 0, i = 1;  // 记录前一个[最长匹配的前缀和后缀]的长度
					lps[0] = 0; // lps[0] 必须是 0
					while (i < M)
					{
						if (pat[i] == pat[len])
						{
							len++;
							lps[i] = len;
							i++;
						}
						else // (pat[i] != pat[len])
						{
							if (len != 0)
							{
								// 这个地方有陷阱. 考虑这个例子 AAACAAAA ,i = 7.
								len = lps[len - 1];

								// 另外, 注意 i 在这个地方并没有增加
							}
							else // 如果 (len == 0)
							{
								lps[i] = 0; //没有一个匹配的
								i++;
							}
						}
					}
				};
				computeLPSArray();
				while (i < N) {
					if (pat[j] == txt[i])
					{
						j++;
						i++;
					}
					if (j == M)
					{
						returnvalue.push_back(i - j);
						j = lps[j - 1];
					}
					else if (pat[j] != txt[i])
					{
						if (j != 0)
							j = lps[j - 1];
						else
							i = i + 1;
					}
				}
				return returnvalue;
			}

			class SetLocal final {//使用时请注意:一个作用域内只能出现一个SetLocal对象，否则析构顺序可能不确定，导致最终local没有被设置回默认的，依然是chs
								  //因为语法问题无法做到ReplaceInfo那样的语法层面单例模式
			public:
				SetLocal() :curLocale(setlocale(LC_ALL, NULL)) {
					setlocale(LC_ALL, "chs");
				}

				~SetLocal() {
					setlocale(LC_ALL, curLocale.c_str());
				}

				SetLocal(const SetLocal&) = delete;

				SetLocal& operator=(const SetLocal&) = delete;

			private:
				std::string curLocale;
			};

			class ReplaceInfo final {
			public:
				ReplaceInfo() = default;

				ReplaceInfo(const ReplaceInfo& input) :whererp(input.whererp), bei_ti_huan_howlong(input.bei_ti_huan_howlong) {}

				ReplaceInfo(ReplaceInfo&& input)noexcept : whererp(std::move(input.whererp)), bei_ti_huan_howlong(input.bei_ti_huan_howlong) {}//这里size_t就不转换了因为他的移动本来就是拷贝，你再转一次还有额外的类型转换开销

				ReplaceInfo& operator=(const ReplaceInfo& input) {
					whererp = input.whererp;
					bei_ti_huan_howlong = input.bei_ti_huan_howlong;
					return *this;
				}

				ReplaceInfo& operator=(ReplaceInfo&& input)noexcept {
					whererp = std::move(input.whererp);
					bei_ti_huan_howlong = input.bei_ti_huan_howlong;
					return *this;
				}

			public:
				void setplace(const std::vector<std::size_t>& input) {
					whererp = input;
				}

				void setplace(std::vector<std::size_t>&& input) {
					whererp = std::move(input);
				}

				void setsize(const std::size_t& input) {
					bei_ti_huan_howlong = input;
				}

				const std::vector<std::size_t>& getplace_ref()const {
					return whererp;
				}

				std::size_t getsize()const {
					return bei_ti_huan_howlong;
				}

			private:
				std::vector<std::size_t> whererp;
				std::size_t bei_ti_huan_howlong;
			};

		}

		std::string insert(const std::string& str, const std::string& input, const std::size_t& wheretoput) {//在 std::string str[wheretoput] 之后插入 std::string input
																											 //wheretoput非法时抛异常
																											 //兼容中文
			auto GetBefore = [&str, &wheretoput] {
				std::string rv;
				rv.reserve(wheretoput);
				for (std::size_t i = 0; i < wheretoput; i++)
					rv.push_back(str[i]);
				return rv;
			};
			auto GetAfter = [&str, &wheretoput] {
				std::string rv;
				rv.reserve(str.size() - wheretoput);
				for (std::size_t i = wheretoput; i < str.size(); i++)
					rv.push_back(str[i]);
				return rv;
			};
			if (wheretoput<0 || wheretoput>str.size()) throw DC::DC_ERROR("index illegal", 0);
			return GetBefore() + input + GetAfter();
		}

		std::string insertCHS(const std::string& str, const std::string& input, const std::size_t& wheretoput) {//在 std::string str[wheretoput] 之后插入 std::string input
																												//wheretoput非法时抛异常
																												//兼容中文
			if (wheretoput < 0) throw DC::DC_ERROR("index illegal", 0);
			std::size_t realput = 0, count_ = 0;
			std::string before, after;
			for (std::size_t i = 0; i < str.size(); i++) {
				if (count_ == wheretoput) { realput = i; }
				if ((str[i] < 0 || str[i]>127) && (str[i + 1] < 0 || str[i + 1]>127)) { i++; count_++; }
				else count_++;
			}
			if (wheretoput >= count_) {
				if (wheretoput == count_) return str + input;
				throw DC::DC_ERROR("index illegal", 0);
			}
			before.reserve(realput);
			for (std::size_t i = 0; i < realput; i++) {
				if (str[i] != NULL) before.push_back(str[i]);
			}
			after.reserve(str.size() - realput);
			for (std::size_t i = 0; i < str.size() - realput; i++) {
				if (str[(realput + i)] != NULL) after.push_back(str[(realput)+i]);
			}
			return before + input + after;
		}

		std::string toUpper(std::string str) {//将 std::string str 中的小写字符转为大写
			transform(str.begin(), str.end(), str.begin(), ::toupper);
			return str;
		}

		std::string toLower(std::string str) {//将 std::string str 中的大写字符转为小写
			transform(str.begin(), str.end(), str.begin(), ::tolower);
			return str;
		}

		inline STRSpace::ReplaceInfo find(const std::string& str, const std::string& findstr) {//使用 KMP 字符串匹配算法找到所有存在于 std::string str 中的 std::string find，并返回它们的位置
			STRSpace::ReplaceInfo rv;
			rv.setsize(findstr.size());
			rv.setplace(STRSpace::KMPSearch(findstr.c_str(), str.c_str()));
			return rv;
		}

		std::string replace(const std::string& str, const STRSpace::ReplaceInfo& info, const std::string& rpword) {//请确保 whererp 中的元素(表示位置的数字)是从小到大有序排列的。
																												   //不能查找替换循环的字符串，比如查找hh，把hh替换为h，这个不work
			if (info.getplace_ref().empty() || str.empty()) return str;
			const std::size_t endsize = str.size() + info.getplace_ref().size()*(rpword.size() - info.getsize());
			std::vector<std::size_t>::const_iterator wherepit = info.getplace_ref().begin();
			std::string TEMP_str;
			for (std::size_t index = 0; TEMP_str.size() < endsize;) {
				if (wherepit != info.getplace_ref().end()) {
					if (index == *wherepit) {
						TEMP_str += rpword;
						index += info.getsize();
						wherepit++;
						continue;
					}
				}
				TEMP_str += str[index];
				index++;
			}
			return TEMP_str;
		}

		inline std::string getSub(const std::string& str, const std::size_t& startpos, const std::size_t& endpos) {//获取子串
																												   //从startpos+1到endpos-1
			if (endpos < startpos || endpos>str.size() || endpos < 0) throw DC::DC_ERROR("pos illegal", 0);
			std::string returnvalue;
			returnvalue.reserve(endpos - startpos);
			for (std::size_t i = startpos + 1; i <= endpos - 1 && i < str.size(); i++)
				returnvalue.push_back(str[i]);
			return returnvalue;
		}

		template <typename TYPE>
		TYPE toType(const std::string &str) {
			TYPE rv;
			std::stringstream sstr(str);
			sstr >> rv;
			if (sstr.fail()) {
				//不需要clear，因为sstr不会再次使用
				throw DC::DC_ERROR("std::stringstream::fail()==true", 0);
			}
			return rv;
		}

		template <>
		const char* toType<const char*>(const std::string &str) {
			return str.c_str();
		}

		template <>
		char* toType<char*>(const std::string &str) = delete;

		template <>
		std::wstring toType<std::wstring>(const std::string& s) {//多线程使用这个必须加锁
			STRSpace::SetLocal sl;
			const char* _Source = s.c_str();
			size_t _Dsize = s.size() + 1;
			std::unique_ptr<wchar_t[]> _Dest(new wchar_t[_Dsize]);
			wmemset(_Dest.get(), NULL, _Dsize);
			mbstowcs(_Dest.get(), _Source, _Dsize);
			std::wstring result = _Dest.get();
			return result;
		}

		template <class T>
		std::string toString(const T& value) {
			std::stringstream sstr;
			sstr << value;
			if (sstr.fail()) {
				//不需要clear，因为sstr不会再次使用
				throw DC::DC_ERROR("std::stringstream::fail()==true", 0);
			}
			return sstr.str();
		}

		template <>
		std::string toString<std::wstring>(const std::wstring& ws) {//多线程使用这个必须加锁
			STRSpace::SetLocal sl;
			const wchar_t* _Source = ws.c_str();
			size_t _Dsize = 2 * ws.size() + 1;
			std::unique_ptr<char[]> _Dest(new char[_Dsize]);
			memset(_Dest.get(), NULL, _Dsize);
			wcstombs(_Dest.get(), _Source, _Dsize);
			std::string result = _Dest.get();
			return result;
		}

	}

}

#endif