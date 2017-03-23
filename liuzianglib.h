#pragma once
#ifndef liuzianglib_liuzianglib
#define liuzianglib_liuzianglib
#include <iostream>
#include <random>
#include <queue>
#include "DC_var.h"
//Version 2.4.1V10
//20170323

#define GET_FIRST_PARAMETERS 0//适用于GetCommandLineParameters

namespace DC {

	typedef std::queue<std::string> PARS_V;
	typedef std::vector<DC::var> ARGS_V;

	static inline int32_t randomer(int32_t s, int32_t b) {//生成介于s和b之间的随机数(包括s与b)
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(s, b);
		return dis(gen);
	}

	template <typename numtype> std::string::size_type Howmuchdig(numtype num) {//返回num的位数，比如num=1000时，返回4
		int32_t i = 0;
		while (num > 1) { num /= 10; i++; }
		if (num == 1) return i + 1; else return i;
	}

	template <typename itemtype> inline void swap(itemtype& a, itemtype& b) {
		itemtype TEMP(std::move(a));
		a = std::move(b);
		b = std::move(TEMP);
	}

	template<typename ...ARGS>
	void GetCommandLineParameters(const char* argv[], ARGS& ...args) {//开始
		PARS_V pars;
		for (std::size_t i = 1; true; i++) {
			if (argv[i] != NULL) pars.push(argv[i]);
			else break;
		}
		GetCommandLineParameters(pars, args...);
	}

	template<typename ...ARGS>
	void GetCommandLineParameters(const int32_t& flag, const char* argv[], ARGS& ...args) {//有选项的开始
		PARS_V pars;
		for (std::size_t i = flag; true; i++) {
			if (argv[i] != NULL) pars.push(argv[i]);
			else break;
		}
		GetCommandLineParameters(pars, args...);
	}

	template<typename T, typename ...ARGS>
	void GetCommandLineParameters(PARS_V& pars, T& item, ARGS& ...args) {//递归中
		if (!pars.empty()) {
			item = pars.front();
			pars.pop();
		}
		GetCommandLineParameters(pars, args...);
	}

	template<typename T>
	void GetCommandLineParameters(PARS_V& pars, T& item) {//终止条件
		if (!pars.empty()) item = pars.front();
	}

	inline ARGS_V GetArgs() {//无参数
		ARGS_V returnvalue;
		return returnvalue;
	}

	template<typename T>
	ARGS_V GetArgs(const T& arg) {//单个参数
		ARGS_V returnvalue;
		returnvalue.push_back(arg);
		return returnvalue;
	}

	template<typename T,typename ...ARGS>
	ARGS_V GetArgs(const T& arg, const ARGS& ...args) {//多个参数
		ARGS_V returnvalue;
		returnvalue.push_back(arg);
		GetArgs(returnvalue, args...);
		return returnvalue;
	}

	template<typename T, typename ...ARGS>
	void GetArgs(ARGS_V& rv, const T& arg, const ARGS& ...args) {//递归中
		rv.push_back(arg);
		GetArgs(rv, args...);
	}

	template<typename T>
	void GetArgs(ARGS_V& rv, const T& arg) {//终止条件
		rv.push_back(arg);
	}

	std::vector<std::string> GetKeyValuePairStr(const std::string& str) {//提取出空格分隔的名值对字符串
		std::string pushthis;
		std::vector<std::string> rv;
		for (const auto&p : str) {
			if (p != ' ')
				pushthis.push_back(p);
			else {
				rv.push_back(pushthis);
				pushthis = "";
			}
		}
		if (!pushthis.empty()) rv.push_back(pushthis);
		return rv;
	}

	class KeyValuePair {//储存单个名值对
	public:
		KeyValuePair() :OK(false), separator('=') {}

		KeyValuePair(const std::string& input) :OK(false), separator('=') {
			Set(input);
		}

		KeyValuePair(std::string&& input) :OK(false), separator('=') {
			auto temp = std::move(input);
			Set(temp);
		}

		void SetSeparator(const char& input) {
			separator = input;
		}

		void Set(const std::string& input) {
			bool OKflag = false;
			std::size_t whereis = 0;

			//找分隔符位置
			for (const auto& p : input) {
				if (p == separator) {
					OKflag = true;
					break;
				}
				whereis++;
			}
			if (OKflag != true) { OK = false; return; }

			//获取字符串
			name = value = "";
			for (std::size_t i = 0; i < whereis; i++) {
				if (input[i] != NULL) name += input[i];
			}
			for (std::size_t i = whereis + 1; i < input.size(); i++) {
				if (input[i] != NULL) value += input[i];
			}
			OK = true;
		}

		bool isSetOK()const {
			return OK;
		}

		std::string GetName()const {
			return name;
		}

		std::string GetValue()const {
			return value;
		}

	private:
		bool OK;
		char separator;
		std::string name, value;
	};

}

#endif