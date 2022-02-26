#include "./phash.hh"

#include <array>
#include <vector>

#include <iostream> // XXX DEBUG

using namespace std;

static inline void zero(size_t len, uint64_t *data) {
	for (size_t i = 0; i < len; i++) *(data++) = 0;
}

static void normalize(
	const unsigned char *src,
	size_t frame_size,
	vector<unsigned char> &normalized,
	size_t norm_dim,
	array<uint8_t, 3> &mean_rgb
) {
	uint32_t h_scale, v_scale;
	uint32_t h_overflow = 0, v_overflow = 0;
	size_t r_sum = 0, g_sum = 0, b_sum = 0;

	// initialize horizontal grid parsing parameters
	if ((frame_size % norm_dim) == 0) {
		h_scale = frame_size / norm_dim;
		h_overflow = norm_dim;
	} else {
		h_scale = frame_size / (norm_dim - 1);
		h_overflow = frame_size % (norm_dim - 1);
	}

	// initialize vertical grid parsing parameters
	if ((frame_size % norm_dim) == 0) {
		v_scale = frame_size / norm_dim;
		v_overflow = norm_dim;
	} else {
		v_scale = frame_size / (norm_dim - 1);
		v_overflow = frame_size % (norm_dim - 1);
	}

	// build reduced grid
	for (uint32_t row = 0; row < (norm_dim - 1); row++) {
		for (uint32_t col = 0; col < (norm_dim - 1); col++) {

			// take RGB pixel sums for block
			size_t blk_sum_r = 0, blk_sum_g = 0, blk_sum_b = 0;
			for (uint32_t i = 1; i <= v_scale; i++) {
				for (uint32_t j = 1; j <= h_scale; j++) {
					auto x = (col * h_scale) + j - 1;
					auto y = (row * v_scale) + i - 1;
					const unsigned char *pixel = &src[(y * frame_size + x) * 3];
					blk_sum_r += pixel[0];
					blk_sum_g += pixel[1];
					blk_sum_b += pixel[2];
				}
			}

			// update running sums
			r_sum += blk_sum_r;
			g_sum += blk_sum_g;
			b_sum += blk_sum_b;

			// calculate block mean
			uint32_t block_divisor = v_scale * h_scale;
			unsigned char *norm_pixel = &normalized[(row * norm_dim + col) * 3];
			norm_pixel[0] = uint8_t(blk_sum_r / block_divisor);
			norm_pixel[1] = uint8_t(blk_sum_g / block_divisor);
			norm_pixel[2] = uint8_t(blk_sum_b / block_divisor);
		}
	}

	// build overflow column
	const uint32_t overflowColumn = h_scale * (norm_dim - 1);
	for (uint32_t row = 0; row < (norm_dim - 1); row++) {

		// take RGB pixel sums for overflow block
		size_t blk_sum_r = 0, blk_sum_g = 0, blk_sum_b = 0;
		for (uint32_t i = 1; i <= v_scale; i++) {
			for (uint32_t j = 1; j <= h_overflow; j++) {
				auto x = overflowColumn + j - 1;
				auto y = (row * v_scale) + i - 1;
				const unsigned char *pixel = &src[(y * frame_size + x) * 3];
				blk_sum_r += pixel[0];
				blk_sum_g += pixel[1];
				blk_sum_b += pixel[2];
			}
		}

		// update running sums
		r_sum += blk_sum_r;
		g_sum += blk_sum_g;
		b_sum += blk_sum_b;

		// calculate block mean
		uint32_t overflow_divisor = v_scale * h_overflow;
		unsigned char *pixel = &normalized[(row * norm_dim + (norm_dim - 1)) * 3];
		pixel[0] = uint8_t(blk_sum_r / overflow_divisor);
		pixel[1] = uint8_t(blk_sum_g / overflow_divisor);
		pixel[2] = uint8_t(blk_sum_b / overflow_divisor);
	}

	// build overflow row
	const uint32_t overflowRow = v_scale * (norm_dim - 1);
	for (uint32_t col = 0; col < (norm_dim - 1); col++) {

		// take RGB pixel sums for overflow block
		size_t blk_sum_r = 0, blk_sum_g = 0, blk_sum_b = 0;
		for (uint32_t i = 1; i <= v_overflow; i++) {
			for (uint32_t j = 1; j <= h_scale; j++) {
				auto x = (col * h_scale) + j - 1;
				auto y = overflowRow + i - 1;
				const unsigned char *pixel = &src[(y * frame_size + x) * 3];
				blk_sum_r += pixel[0];
				blk_sum_g += pixel[1];
				blk_sum_b += pixel[2];
			}
		}

		// update running sums
		r_sum += blk_sum_r;
		g_sum += blk_sum_g;
		b_sum += blk_sum_b;

		// calculate block mean
		uint32_t overflow_divisor = v_overflow * h_scale;
		unsigned char *pixel = &normalized[((norm_dim - 1) * norm_dim + col) * 3];
		pixel[0] = uint8_t(blk_sum_r / overflow_divisor);
		pixel[1] = uint8_t(blk_sum_g / overflow_divisor);
		pixel[2] = uint8_t(blk_sum_b / overflow_divisor);
	}

	// build overflow row/col
	size_t blk_sum_r = 0, blk_sum_g = 0, blk_sum_b = 0;
	for (uint32_t i = 1; i <= v_overflow; i++) {
		for (uint32_t j = 1; j <= h_overflow; j++) {
			auto y = overflowRow + i - 1;
			auto x = overflowColumn + j - 1;
			const unsigned char *pixel = &src[(y * frame_size + x) * 3];
			blk_sum_r += pixel[0];
			blk_sum_g += pixel[1];
			blk_sum_b += pixel[2];
		}
	}

	// wrap-up overflow normalization
	r_sum += blk_sum_r;
	g_sum += blk_sum_g;
	b_sum += blk_sum_b;
	uint32_t overflow_divisor = v_overflow * h_overflow;

	unsigned char *pixel = &normalized[((norm_dim - 1) * norm_dim + (norm_dim - 1)) * 3];

	pixel[0] = uint8_t(blk_sum_r / overflow_divisor);
	pixel[1] = uint8_t(blk_sum_g / overflow_divisor);
	pixel[2] = uint8_t(blk_sum_b / overflow_divisor);

	uint32_t img_divisor = frame_size * frame_size;

	mean_rgb[0] = uint8_t(r_sum / img_divisor);
	mean_rgb[1] = uint8_t(g_sum / img_divisor);
	mean_rgb[2] = uint8_t(b_sum / img_divisor);
}

static void compute(
	ddb::phash::detail::hash_channels<uint64_t *> &result,
	const vector<unsigned char> &normalized,
	size_t norm_dim,
	const array<uint8_t, 3> &mean
) {
	const uint32_t luminance = (
		  0.2126 * uint32_t(mean[0])
		+ 0.7152 * uint32_t(mean[1])
		+ 0.0722 * uint32_t(mean[2])
	);

	const uint8_t gray_mean = uint8_t(
		  (uint32_t(mean[0])
		+ uint32_t(mean[1])
		+ uint32_t(mean[2]))
		/ 3
	);

	// iterate through normalized grid
	for (uint32_t i = 0; i < norm_dim; i++) {
		for (uint32_t j = 0; j < norm_dim; j++) {
			uint32_t position = (i * norm_dim) + j;
			uint32_t bucket = position / 64;
			uint32_t iterator = position % 64;

			const unsigned char *pixel = &normalized[position * 3];

			// compute RGB hash
			if (pixel[0] >= mean[0]) result.red[bucket] |= 1 << iterator;
			if (pixel[1] >= mean[1]) result.green[bucket] |= 1 << iterator;
			if (pixel[2] >= mean[2]) result.blue[bucket] |= 1 << iterator;

			// compute luminance hash
			const uint32_t pix_luminance = (
				  0.2126 * uint32_t(pixel[0])
				+ 0.7152 * uint32_t(pixel[1])
				+ 0.0722 * uint32_t(pixel[2])
			);

			if (pix_luminance >= luminance) {
				result.luminance[bucket] |= 1 << iterator;
			}

			// compute grayscale hash
			const uint8_t pix_mean = uint8_t(
				  (uint32_t(pixel[0])
				+ uint32_t(pixel[1])
				+ uint32_t(pixel[2]))
				/ 3
			);
			if (pix_mean >= gray_mean) {
				result.grayscale[bucket] |= 1 << iterator;
			}

			// compute combined hash 1
			uint8_t majority = (
				  uint8_t(pixel[0] >= mean[0])
				+ uint8_t(pixel[1] >= mean[1])
				+ uint8_t(pixel[2] >= mean[2])
			);
			if (majority == 0 || majority == 2) {
				result.combined1[bucket] |= 1 << iterator;
			}

			// compute combined hash 2
			if (majority == 1 || majority == 3) {
				result.combined2[bucket] |= 1 << iterator;
			}
		}
	}
}

void ddb::phash::detail::digest_raw(
	hash_channels<uint64_t *> &result,
	const unsigned char *rgb8,
	size_t frame_size,
	size_t hash_length,
	uint64_t normalization_dimension
) {
	zero(hash_length, result.red);
	zero(hash_length, result.green);
	zero(hash_length, result.blue);
	zero(hash_length, result.luminance);
	zero(hash_length, result.grayscale);
	zero(hash_length, result.combined1);
	zero(hash_length, result.combined2);

	vector<unsigned char> normalized;
	normalized.resize(normalization_dimension * normalization_dimension * 3);

	array<uint8_t, 3> mean_rgb;
	normalize(
		rgb8,
		frame_size,
		normalized,
		normalization_dimension,
		mean_rgb
	);

	for (uint64_t y = 0; y < normalization_dimension; y++) {
		for (uint64_t x = 0; x < normalization_dimension; x++) {
			const unsigned char *pixel = &normalized[(y * normalization_dimension + x) * 3];
			std::cout << "\x1b[48;2;"
				<< (int)pixel[0] << ";"
				<< (int)pixel[1] << ";"
				<< (int)pixel[2] << "m ";
		}
		std::cout << "\x1b[m\n";
	}

	compute(
		result,
		normalized,
		normalization_dimension,
		mean_rgb
	);
}
