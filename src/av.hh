#ifndef DDB__AV__HH
#define DDB__AV__HH
#pragma once

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>
#include <string>
#include <set>
#include <utility>
#include <vector>
#include <memory>

struct AVFormatContext;

namespace ddb::av {

template <unsigned int size = 256>
using frame = std::array<unsigned char, size * size>;

struct codec_info {
	codec_info() = default;
	explicit inline codec_info(std::string id, std::string description)
	: id(std::move(id))
	, description(std::move(description))
	{}

	std::string id;
	std::string description;
	std::set<std::string> mime_types;
};

class stream {
public:
	static constexpr std::size_t buffer_size = 4096;

	enum whence {
		BEGINNING = SEEK_SET,
		RELATIVE = SEEK_CUR,
		END = SEEK_END
	};
private:
	AVFormatContext *avctx;
	bool detected;

	static int read_packet(void *, unsigned char *, int);
	static long seek_packet(void *, std::int64_t, int);

	virtual int read(unsigned char *buf, long bufsize) = 0;
	virtual bool seek(long offset, whence) = 0;
	virtual long tell() = 0;
protected:
	stream();
public:
	virtual ~stream();

	bool init();
	bool initialized() const noexcept;
};

std::vector<codec_info> get_codecs();

void init();

}

#endif
