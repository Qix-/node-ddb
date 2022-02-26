#ifndef DDB__PHASH__HH
#define DDB__PHASH__HH
#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

/*
	This implementation is based heavily on
	https://github.com/alangshur/perceptual-dct-hash
*/

namespace ddb::phash {

namespace detail {
	template <typename T>
	struct hash_channels {
		T red;
		T green;
		T blue;
		T luminance;
		T grayscale;
		T combined1;
		T combined2;
	};

	void digest_raw(
		hash_channels<std::uint64_t *> &result,
		const unsigned char *rgb8,
		std::size_t frame_size,
		std::size_t hash_length,
		std::uint64_t normalization_dimension
	);
}

constexpr unsigned long segment_size = 64;

template <std::size_t frame_size>
struct hash_result : public detail::hash_channels<
	std::array<std::uint64_t, frame_size * frame_size / segment_size>
> {
	static constexpr std::size_t hash_length = frame_size * frame_size / segment_size;
};

struct hash_error_result : public detail::hash_channels<float> {};

template <std::size_t frame_size>
void digest(
	const std::array<unsigned char, frame_size * frame_size * 3> &rgb8,
	hash_result<frame_size> &result,
	std::uint64_t normalization_dimension = 32
) {
	detail::hash_channels<std::uint64_t *> ptrs;

	ptrs.red = result.red.data();
	ptrs.green = result.green.data();
	ptrs.blue = result.blue.data();
	ptrs.luminance = result.luminance.data();
	ptrs.grayscale = result.grayscale.data();
	ptrs.combined1 = result.combined1.data();
	ptrs.combined2 = result.combined2.data();

	detail::digest_raw(
		ptrs,
		rgb8.data(),
		frame_size,
		hash_result<frame_size>::hash_length,
		normalization_dimension
	);
}

}

#endif
