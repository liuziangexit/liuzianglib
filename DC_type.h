#pragma once
#ifndef liuzianglib_type
#define liuzianglib_type
#include <iostream>
//Version 2.4.21V14
//20170721

//This file only support MSVC

namespace DC {

	using byte_t = unsigned char;
	using size_t = unsigned int;

#ifdef _WIN64

	typedef int64_t pos_type;

#else

	typedef int32_t pos_type;

#endif
	
}

#endif