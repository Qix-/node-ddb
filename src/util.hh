#ifndef DDB__UTIL__HH
#define DDB__UTIL__HH
#pragma once

#include <cstdlib>
#include <cstdio>
#include <functional>

#define DDB_PASTE_(x, y) x ## y
#define DDB_PASTE(x, y) DDB_PASTE_(x, y)

#define DDB_COUNTOF(x) (sizeof((x))/sizeof((x)[0]))

namespace ddb {

class defer {
	std::function<void()> fn;
public:
	template <typename T>
	inline defer(T fn)
	: fn(fn)
	{}

	inline ~defer() {
		if (fn) fn();
	}
};

#define DEFER ddb::defer DDB_PASTE(_def__, __LINE__) =

}

#endif
