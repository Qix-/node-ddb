#include "./av.hh"
#include "./error.hh"
#include "./util.hh"

extern "C" {
#	include <libavutil/opt.h>
#	include <libavformat/avio.h>
#	include <libavcodec/avcodec.h>
#	include <libavformat/avformat.h>
#	include <libavcodec/mediacodec.h>
#	include <libavutil/imgutils.h>
#	include <libswscale/swscale.h>
}

#include <cassert>
#include <cstdint>
#include <algorithm>

const ddb::av::av_category ddb::av::av_category::inst;

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
, stream_id(-1)
{}

void ddb::av::stream::init(std::error_code &err) {
	if (!avctx) {
		avctx = avformat_alloc_context();
		if (avctx == nullptr) return err.assign(ddb::ERR_NO_MEM, ddb::ddb_category::inst);
		avctx->pb = nullptr;
	}

	if (avctx->pb == nullptr) {
		unsigned char *buf = (unsigned char *)av_malloc(buffer_size);
		if (buf == nullptr) return err.assign(ddb::ERR_NO_MEM, ddb::ddb_category::inst);

		avctx->pb = avio_alloc_context(
			buf, buffer_size,
			0,
			(void *) this,
			&read_packet,
			nullptr,
			&seek_packet
		);

		if (avctx->pb == nullptr) return err.assign(ddb::ERR_NO_MEM, ddb::ddb_category::inst);

		int r = avformat_open_input(&avctx, nullptr, nullptr, nullptr);
		if (r < 0) {
			return err.assign(r, ddb::av::av_category::inst);
		}
	}

	if (detected) return;

	int r = avformat_find_stream_info(avctx, nullptr);
	detected = r >= 0;

	if (r < 0) err.assign(r, ddb::av::av_category::inst);

	if (detected) {
		stream_id = -1;
		for (unsigned int i = 0; i < avctx->nb_streams; i++) {
			if (avctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
				stream_id = (int) i;
				break;
			}
		}

		if (stream_id == -1) {
			err.assign(ddb::ERR_NO_VIDEO, ddb::ddb_category::inst);
			return;
		}
	}
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

		avformat_close_input(&avctx);
		avformat_free_context(avctx);
		avctx = nullptr;
	}
}

bool ddb::av::stream::initialized() const noexcept {
	return detected && avctx && avctx->pb && avctx->pb->buffer;
}

ddb::av::frame::frame(const unsigned char *begin, const unsigned char *end) {
	std::copy(begin, end, pixels.begin());
}

void ddb::av::init() {
#if (LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100))
	av_register_all();
#endif
#if (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 10, 100))
	avcodec_register_all();
#endif
}

const char * ddb::av::av_category::name() const noexcept {
	return "libav";
}

std::string ddb::av::av_category::message(int condition) const {
	std::string res;
	res.resize(AV_ERROR_MAX_STRING_SIZE);
	if (av_strerror(condition, res.data(), AV_ERROR_MAX_STRING_SIZE) < 0) {
		res = "<unknown av error>";
	} else {
		res.shrink_to_fit();
	}
	return res;
}

std::vector<ddb::av::frame> ddb::av::stream::decode(std::error_code &err) {
	assert(stream_id >= 0);
	assert((unsigned)stream_id < avctx->nb_streams);

	if (!initialized()) {
		err.assign(ddb::ERR_NOT_INITIALIZED, ddb::ddb_category::inst);
		return {};
	}

	struct decoder_session {
		std::vector<frame> frames;
		AVStream *stream = nullptr;
		AVCodec *decoder = nullptr;
		AVCodecContext *codec = nullptr;
		AVFrame *src_frame = nullptr;
		AVFrame *dst_frame = nullptr;
		AVPacket *packet = nullptr;
		SwsContext *sws = nullptr;
		uint8_t *dst_buffer = nullptr;

		~decoder_session() {
			if (codec) avcodec_free_context(&codec);
			if (src_frame) av_frame_free(&src_frame);
			if (dst_frame) av_frame_free(&dst_frame);
			if (packet) av_packet_free(&packet);
			if (sws) sws_freeContext(sws);
			if (dst_buffer) av_free(dst_buffer);
		}

		int decode_packet(AVPacket *packet) {
			int r = avcodec_send_packet(codec, packet);
			if (r < 0) return r;

			while (r >= 0) {
				r = avcodec_receive_frame(codec, src_frame);
				if (r == AVERROR_EOF || r == AVERROR(EAGAIN)) return 0;

				r = sws_scale(
					sws,
					src_frame->data,
					src_frame->linesize,
					0,
					src_frame->height,
					dst_frame->data,
					dst_frame->linesize
				);

				av_frame_unref(src_frame);

				if (r >= 0) {
					frames.emplace_back(
						dst_buffer,
						dst_buffer + (frame::frame_size * frame::frame_size * 3)
					);
				}
			}

			return r;
		}
	} session;

	session.stream = avctx->streams[stream_id];

	session.decoder = avcodec_find_decoder(session.stream->codecpar->codec_id);
	if (session.decoder == nullptr) {
		err.assign(ddb::ERR_UNKNOWN_DECODER, ddb_category::inst);
		return {};
	}

	session.codec = avcodec_alloc_context3(session.decoder);
	if (session.codec == nullptr) {
		err.assign(ddb::ERR_NO_MEM, ddb_category::inst);
		return {};
	}

	int r = avcodec_parameters_to_context(session.codec, session.stream->codecpar);
	if (r < 0) {
		err.assign(r, av::av_category::inst);
		return {};
	}

	r = avcodec_open2(session.codec, session.decoder, NULL);
	if (r < 0) {
		err.assign(r, av::av_category::inst);
		return {};
	}

	session.src_frame = av_frame_alloc();
	if (!session.src_frame) {
		err.assign(ddb::ERR_NO_MEM, ddb_category::inst);
		return {};
	}

	session.dst_frame = av_frame_alloc();
	if (!session.dst_frame) {
		err.assign(ddb::ERR_NO_MEM, ddb_category::inst);
		return {};
	}

	int num_bytes = avpicture_get_size(AV_PIX_FMT_RGB24, frame::frame_size, frame::frame_size);
	assert(num_bytes == (frame::frame_size * frame::frame_size * 3));
	session.dst_buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t));
	avpicture_fill(
		(AVPicture *)session.dst_frame,
		session.dst_buffer,
		AV_PIX_FMT_RGB24,
		frame::frame_size,
		frame::frame_size
	);

	session.packet = av_packet_alloc();
	if (!session.packet) {
		err.assign(ddb::ERR_NO_MEM, ddb_category::inst);
		return {};
	}

	session.sws = sws_getContext(
		session.codec->width,
		session.codec->height,
		session.codec->pix_fmt,
		frame::frame_size,
		frame::frame_size,
		AV_PIX_FMT_RGB24,
		0,
		nullptr,
		nullptr,
		nullptr
	);

	if (session.sws == nullptr) {
		err.assign(ddb::ERR_INVALID_SWS, ddb_category::inst);
		return {};
	}

	// Decode
	while ((r = av_read_frame(avctx, session.packet)) >= 0) {
		if (session.packet->stream_index == stream_id)
			r = session.decode_packet(session.packet);
		av_packet_unref(session.packet);
		if (r < 0) break;
	}

	if (r != AVERROR_EOF) {
		err.assign(r, av::av_category::inst);
		return {};
	}

	// flush decoders
	r = session.decode_packet(nullptr);
	if (r < 0) {
		err.assign(r, av::av_category::inst);
		return {};
	}

	return session.frames;
}

void ddb::av::stream::dump(std::error_code &err) const {
	if (!initialized()) {
		return err.assign(ddb::ERR_NOT_INITIALIZED, ddb::ddb_category::inst);
	}
	av_dump_format(avctx, stream_id, "<input>", 0);
}
