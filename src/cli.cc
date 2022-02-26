#include "./av.hh"
#include "./phash.hh"

#include <fstream>
#include <iostream>
#include <filesystem>
#include <stdexcept>

class file_stream : public ddb::av::stream {
	std::ifstream ifs;

	virtual int read(unsigned char *buf, long bufsize) override {
		if (ifs.eof()) return 0;
		if (ifs.fail()) return -1;
		if (ifs.good()) ifs.read((char *) buf, bufsize);
		if (ifs.fail() && !ifs.eof()) return -1;
		if (ifs.eof()) {
			ifs.clear(ifs.rdstate() & ~ifs.failbit);
		}
		return ifs.gcount();
	}

	virtual bool seek(long offset, whence w) override {
		std::ios_base::seekdir dir;
		switch (w) {
			case ddb::av::stream::BEGINNING:
				dir = ifs.beg;
				break;
			case ddb::av::stream::RELATIVE:
				dir = ifs.cur;
				break;
			case ddb::av::stream::END:
				dir = ifs.end;
				break;
		}

		ifs.seekg(offset, dir);
		return true;
	}

	virtual long tell() override {
		if (ifs.fail()) return -1;
		return ifs.tellg();
	}
public:
	file_stream(std::filesystem::path pth)
	: ddb::av::stream()
	{
		ifs.open(pth, std::ios_base::binary);
		if (ifs.fail()) {
			throw std::runtime_error("failed to open file");
		}
	}
};

int main(int argc, char *argv[]) {
	ddb::av::init();

	if (argc <= 1) {
		auto codec_list = ddb::av::get_codecs();

		if (codec_list.empty()) {
			std::cerr << "warning: no codecs available\n";
		} else {
			for (const auto &codec : codec_list) {
				std::cerr << "codec: " << codec.id << " (" << codec.description << ")\n";
				if (codec.mime_types.empty()) {
					std::cerr << "\t<no mime types>\n";
				} else {
					for (const auto &mime : codec.mime_types) {
						std::cerr << "\t" << mime << "\n";
					}
				}
			}
		}
	}

	if (argc != 2) {
		std::cerr << "error: no inputs / too many inputs given (need exactly one file)\n";
		return 2;
	}

	file_stream stream{ argv[1] };

	std::error_code err;
	stream.init(err);
	if (err) {
		std::cerr << "error: failed to initialize or detect file: "
			<< err << ": " << err.message() << "\n";
		return 2;
	}

	stream.dump(err);
	if (err) {
		std::cerr << "failed to dump format info: "
			<< err << ": " << err.message() << "\n";
		return 1;
	}

	std::vector<ddb::av::frame> frames = stream.decode(err);
	if (err) {
		std::cerr << "failed to decode: "
			<< err << ": " << err.message() << "\n";
		return 1;
	}

	std::cerr << "# frames: " << frames.size() << "\n";

	if (frames.size() == 0) {
		std::cerr << "error: no frames\n";
		return 1;
	}

	// dump ANSI
	for (const auto &frame : frames) {
		for (std::size_t y = 0; y < ddb::av::frame::frame_size; y++) {
			for (std::size_t x = 0; x < ddb::av::frame::frame_size; x++) {
				const unsigned char *pixel = &frame.pixels[(y * ddb::av::frame::frame_size + x) *3];
				std::cout
					<< "\x1b[48;2;"
					<< (int)pixel[0] << ";"
					<< (int)pixel[1] << ";"
					<< (int)pixel[2] << "m ";
			}
			std::cout << "\x1b[m\n";
		}
		std::cout << "\x1b[m\n";

		ddb::phash::hash_result<ddb::av::frame::frame_size> result;
		ddb::phash::digest(frame.pixels, result);

		static const auto dump_result = [](const char *name, const decltype(result.red) &result) {
			std::cerr << name << ":\n";
			for (const auto &val : result) {
				std::cerr << "\t" << val << "\n";
			}
		};

		dump_result("red", result.red);
		dump_result("green", result.green);
		dump_result("blue", result.blue);
		dump_result("luminance", result.luminance);
		dump_result("grayscale", result.grayscale);
		dump_result("combined1", result.combined1);
		dump_result("combined2", result.combined2);
	}

	return 0;
}
