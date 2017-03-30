#pragma once
#ifndef liuzianglib_timer
#define liuzianglib_timer
#include <time.h>
//Version 2.4.2
//20170330

namespace DC {

	class timer {//调试器里暂停时也算
				 //非线程安全
	public:
		timer() {
			status = false;
			reset();
		}

	public:
		inline void start() {
			if (status == true) return;
			record = clock();
			status = true;
		}

		inline void stop() {
			if (status == false) return;
			res += (clock() - record);
			status = false;
		}

		inline void reset() {
			status = false;
			res = 0;
		}

		inline time_t getsecond() {
			return getclock() / CLOCKS_PER_SEC;
		}

		inline time_t getms() {//这个和平台相关，在VS15下clock_t就是毫秒,其他平台可能需要修改
			return getclock();
		}

	private:
		inline clock_t getclock() {
			if (status == false)
				return res;
			decltype(res) temp = res;
			temp += (clock() - record);
			return temp;
		}

	private:
		bool status;//true==running,false==stop
		clock_t record, res;
	};

}

#endif