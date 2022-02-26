#include "./error.hh"

const ddb::ddb_category ddb::ddb_category::inst;

const char * ddb::ddb_category::name() const noexcept {
	return "ddb";
}

std::string ddb::ddb_category::message(int condition) const {
	switch ((ddb::error_code) condition) {
		case ERR_OK: return "ok";
		case ERR_NO_MEM: return "out of memory";
		case ERR_NOT_INITIALIZED: return "stream not initialized";
		case ERR_NO_VIDEO: return "stream contains no video";
		case ERR_UNKNOWN_DECODER: return "unknown or unsupported decoder";
		case ERR_INVALID_SWS: return "scaling/pixel format conversion is not possible";
	}

	return "<unknown>";
}
