#pragma once
#ifndef liuzianglib_type
#define liuzianglib_type
#include <iostream>
//Version 2.4.2
//20170330

namespace DC {

#ifdef _WIN64

	typedef int64_t pos_type;

#else

	typedef int32_t pos_type;

#endif
	
}

#endif