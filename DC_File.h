#pragma once
#ifndef liuzianglib_File
#define liuzianglib_File
#include <string>
#include "DC_Exception.h"
//Version 2.4.2V28
//20170502

#define ERROR_CANTOPENFILE DC::DC_ERROR(filename, "CAN NOT OPEN FILE")
#define ERROR_CANTGETSIZE DC::DC_ERROR(filename, "CAN NOT GET FILE SIZE")

namespace DC {

	namespace File {
		//某些函数有ptr重用的重载版本
		//打开失败抛异常

		class text;
		class binary;

		class file_ptr final {//管理C语言文件指针生命周期的包装器
							  //可以通过if(file_ptr)的方式判断file_ptr是否已经打开
							  //可以在使用时手动fclose(file_ptr::get())，这不会导致未定义行为
							  //仅支持移动拷贝
							  //某些时候发现在使用此类的过程中有1(VS内存查看器中的“分配(差异)”)内存没有回收，这不是内存泄漏。
		public:
			file_ptr() :fp(nullptr) {}

			file_ptr(FILE* input) :fp(input) {}

			file_ptr(const file_ptr&) = delete;

			file_ptr(file_ptr&& input)noexcept {
				clear();
				this->fp = input.fp;
				input.fp = nullptr;
			}

			~file_ptr() {
				clear();
			}

			file_ptr& operator=(const file_ptr&) = delete;

			file_ptr& operator=(file_ptr&& input)noexcept {
				clear();
				this->fp = input.fp;
				input.fp = nullptr;
				return *this;
			}

			operator bool()const {
				if (fp == NULL || fp == nullptr) return false;
				return true;
			}

			inline FILE* get()const {
				return fp;
			}

			inline void reset(FILE* input) {
				clear();
				fp = input;
			}

		private:
			inline void clear() {
				if (fp == NULL || fp == nullptr) return;
				fclose(fp);
			}

		private:
			FILE *fp;
		};

		inline void del(const std::string& filename)noexcept {
			remove(filename.c_str());
		}

		inline bool exists(const std::string& filename)noexcept {
			file_ptr ptr(fopen(filename.c_str(), "r"));
			return (bool)ptr;
		}

		template<typename FLAG>
		inline bool exists(const std::string& filename, file_ptr& inputptr)noexcept;

		template<>
		inline bool exists<text>(const std::string& filename, file_ptr& inputptr)noexcept {//同时判断是否存在和是否能打开
			file_ptr ptr(fopen(filename.c_str(), "r"));
			inputptr = std::move(ptr);
			return (bool)inputptr;
		}

		template<>
		inline bool exists<binary>(const std::string& filename, file_ptr& inputptr)noexcept {//同时判断是否存在和是否能打开
			file_ptr ptr(fopen(filename.c_str(), "rb"));
			inputptr = std::move(ptr);
			return (bool)inputptr;
		}

		std::size_t getSize(const std::string& filename) {//获取文件长度
			file_ptr ptr;
			if (!exists<text>(filename, ptr)) throw ERROR_CANTOPENFILE;
			fseek(ptr.get(), 0L, SEEK_END);
			const auto& rv = ftell(ptr.get());
			if (rv < 0) throw ERROR_CANTGETSIZE;
			return rv;//有符号无符号不匹配可以忽略，这里已经处理过那种错误了
		}

		template<typename FLAG = text>
		std::string read(const std::string& filename) {
			static_assert(std::is_same<std::decay_t<FLAG>, text>::value || std::is_same<std::decay_t<FLAG>, binary>::value, "flag illegal");
			file_ptr ptr;
			if (!exists<FLAG>(filename, ptr)) throw ERROR_CANTOPENFILE;
			std::string returnvalue;
			auto&& ch = fgetc(ptr.get());
			while (ch != EOF) {
				returnvalue.push_back(ch);
				ch = fgetc(ptr.get());
			}
			return returnvalue;
		}

		template<typename FLAG = text>
		bool write(const std::string& filename, const std::string& write)noexcept;

		template<>
		bool write<text>(const std::string& filename, const std::string& write)noexcept {//覆盖写入
			file_ptr ptr(fopen(filename.c_str(), "w"));
			if (!ptr) return false;
			if (fprintf(ptr.get(), "%s", write.c_str()) == -1) return false;
			return true;
		}

		template<>
		bool write<binary>(const std::string& filename, const std::string& write)noexcept {//覆盖写入
			file_ptr ptr(fopen(filename.c_str(), "wb"));
			if (!ptr) return false;
			if (fwrite(write.c_str(), 1, write.size(), ptr.get()) == -1) return false;
			return true;
		}

		template<typename FLAG = text>
		bool writeAppend(const std::string& filename, const std::string& write)noexcept;

		template<>
		bool writeAppend<text>(const std::string& filename, const std::string& write)noexcept {
			file_ptr ptr(fopen(filename.c_str(), "a"));
			if (!ptr) return false;
			if (fprintf(ptr.get(), "%s", write.c_str()) == -1) return false;
			return true;
		}

		template<>
		bool writeAppend<binary>(const std::string& filename, const std::string& write)noexcept {
			file_ptr ptr(fopen(filename.c_str(), "ab"));
			if (!ptr) return false;
			if (fwrite(write.c_str(), 1, write.size(), ptr.get()) == -1) return false;
			return true;
		}

	}

}

#endif