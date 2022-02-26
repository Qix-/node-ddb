#ifndef DDB__ERROR__HH
#define DDB__ERROR__HH
#pragma once

#include <system_error>

namespace ddb {

enum error_code {
	ERR_OK = 0,
	ERR_NO_MEM,
	ERR_NOT_INITIALIZED,
	ERR_NO_VIDEO,
	ERR_UNKNOWN_DECODER,
	ERR_INVALID_SWS
};

class ddb_category : public std::error_category {
	constexpr ddb_category() : std::error_category{} {}
public:
	virtual const char * name() const noexcept override;
	virtual std::string message(int condition) const override;

	static const ddb_category inst;
};

}

#endif
