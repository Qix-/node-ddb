#include "./av.hh"

extern "C" {
#	include <libavutil/opt.h>
#	include <libavformat/avio.h>
#	include <libavcodec/avcodec.h>
#	include <libavformat/avformat.h>
#	include <libavcodec/mediacodec.h>
}

#include <algorithm>

std::vector<ddb::av::codec_info> ddb::av::get_codecs() {
	std::vector<codec_info> result;

	const AVCodecDescriptor *codec = nullptr;
	while ((codec = avcodec_descriptor_next(codec))) {
		if (codec->type != AVMEDIA_TYPE_VIDEO) continue;

		auto &entry = result.emplace_back(codec->name, codec->long_name);

		const char * const *mime_type = codec->mime_types;
		if (mime_type) {
			for (; *mime_type; mime_type++) {
				entry.mime_types.insert(*mime_type);
			}
		}
	}

	return result;
}

long ddb::av::stream::seek_packet(void *thisptr, std::int64_t pos, int w) {
	w &= ~AVSEEK_FORCE; // we don't care about this.

	// we don't care to support this; it'll ask us again to
	// seek to the end.
	if (w == AVSEEK_SIZE) return -1;
	if (w != RELATIVE) pos = std::max(pos, 0l);

	bool ok = ((ddb::av::stream *)thisptr)->seek(pos, (whence) w);
	if (!ok) return AVERROR_UNKNOWN;

	long r = ((ddb::av::stream *)thisptr)->tell();
	if (r < 0) return AVERROR_UNKNOWN;

	return r;
}

int ddb::av::stream::read_packet(void *thisptr, unsigned char *buf, int size) {
	int numbytes = ((ddb::av::stream *)thisptr)->read(buf, std::min(size, (int) buffer_size));
	if (numbytes == 0) return AVERROR_EOF;
	if (numbytes < 0) return AVERROR_UNKNOWN;
	return numbytes;
}

ddb::av::stream::stream()
: avctx(nullptr)
, detected(false)
{}

bool ddb::av::stream::init() {
	if (!avctx) {
		avctx = avformat_alloc_context();
		if (avctx == nullptr) return false;
		avctx->pb = nullptr;
	}

	if (avctx->pb == nullptr) {
		unsigned char *buf = (unsigned char *)av_malloc(buffer_size);
		if (buf == nullptr) return false;

		avctx->pb = avio_alloc_context(
			buf, buffer_size,
			0,
			(void *) this,
			&read_packet,
			nullptr,
			&seek_packet
		);

		if (avctx->pb == nullptr) return false;

		if (avformat_open_input(&avctx, nullptr, nullptr, nullptr) < 0) {
			return false;
		}
	}

	if (detected) return true;

	detected = avformat_find_stream_info(avctx, nullptr) >= 0;
	return detected;
}

ddb::av::stream::~stream() {
	if (avctx) {
		if (avctx->pb) {
			if (avctx->pb->buffer) {
				av_free(avctx->pb->buffer);
				avctx->pb->buffer = nullptr;
			}

			avio_context_free(&avctx->pb);
		}

		avformat_free_context(avctx);
		avctx = nullptr;
	}
}

bool ddb::av::stream::initialized() const noexcept {
	return detected && avctx && avctx->pb && avctx->pb->buffer;
}

void ddb::av::init() {
#if (LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100))
	av_register_all();
#endif
#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 10, 100))
	avcodec_register_all();
#endif
}
