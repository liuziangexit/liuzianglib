#pragma once
#ifndef liuzianglib_bit_array
#define liuzianglib_bit_array
#include <cstddef>
#include <climits>
#include <type_traits>
//Version 2.4.22V21
//20180611

namespace DC {

	template <typename _Ty>
	class bit_array {
		static_assert(std::is_unsigned<_Ty>::value, "value_type must be a unsigned arithmetic type");

	public:
		using value_type = _Ty;
		using size_type = std::size_t;

	public:
		bool operator[](std::size_t index)const {
			return this->at(index);
		}

	public:
		void set(std::size_t index, bool new_value) {
			if (new_value)
				value |= (value_type(1) << index);
			else
				value ^= value & (value_type(1) << index);
		}

		void set(bool new_value) {
			value = 0;
			if (new_value)
				value = ~value;
		}

		bool at(std::size_t index)const {
			return (value & (value_type(1) << index)) == (value_type(1) << index);
		}

		constexpr std::size_t size()const {
			return sizeof(value_type) * CHAR_BIT;
		}

		constexpr value_type get_value()const {
			return this->value;
		}

		constexpr void set_value(value_type v) {
			this->value = v;
		}

	private:
		value_type value = 0;
	};

}

#endif
