#include "./av.hh"

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
		std::cerr << "error: failed to initialize or detect file: " << err << "\n";
		return 2;
	}

	std::vector<ddb::av::frame> frames = stream.decode(err);
	if (err) {
		std::cerr << "failed to decode: " << err << "\n";
		return 1;
	}

	std::cerr << "# frames: " << frames.size() << "\n";

	return 0;
}
