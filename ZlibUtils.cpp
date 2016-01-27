#include "ZlibUtils.h"
#include "Common.h"

#include <zlib.h>

#include <assert.h>

#ifdef max
#	undef max
#endif

#define COMPRESS_CHUNK_SIZE (256 * 1024)

namespace HQRemote {
	void zlibCompress(const IData& uncompressedData, int level, GrowableData& dst) {
		if (level == 0)
			level = Z_DEFAULT_COMPRESSION;

		z_stream sz;
		memset(&sz, 0, sizeof(sz));

		//placeholder for uncompressed size
		auto uncompressedSizeOff = dst.size();
		assert(uncompressedSizeOff % sizeof(uint64_t) == 0);

		dst.expand(sizeof(uint64_t));

		//compress the data
		if (deflateInit(&sz, level) != Z_OK)
			throw std::runtime_error("deflateInit failed");
		if (uncompressedData.size() > std::numeric_limits<uint32_t>::max())
			throw std::runtime_error("uncompressed data too big");

		int re;
		unsigned char buffer[COMPRESS_CHUNK_SIZE];
		sz.next_in = (unsigned char*)uncompressedData.data();
		sz.avail_in = uncompressedData.size();
		sz.next_out = buffer;
		sz.avail_out = sizeof(buffer);

		while ((re = deflate(&sz, Z_FINISH)) != Z_STREAM_END && re == Z_OK) {
			dst.push_back(buffer, sizeof(buffer) - sz.avail_out);

			sz.avail_out = sizeof(buffer);
		}

		dst.push_back(buffer, sizeof(buffer) - sz.avail_out);

		//finalize
		deflateEnd(&sz);
		uint64_t& uncompressedSize = *(uint64_t*)(dst.data() + uncompressedSizeOff);

		uncompressedSize = uncompressedData.size();

		if (re != Z_STREAM_END)
		{
			throw std::runtime_error("compression failed");
		}
	}

	DataRef zlibDecompress(const IData& src) {
		const uint64_t& uncompressedSize = *(const uint64_t*)(src.data());
		const unsigned char* compressedData = (const unsigned char*)(&uncompressedSize + sizeof(uncompressedSize));
		_ssize_t compressedSize = (_ssize_t)src.size() - (_ssize_t)(src.data() - compressedData);
		if (compressedSize < 0)
			throw  std::runtime_error("Size is too small for decompression");

		auto decompressedData = std::make_shared<CData>(uncompressedSize);
		uLongf decompressedSize = decompressedData->size();
		if (uncompress(decompressedData->data(), &decompressedSize, compressedData, compressedSize) != Z_OK
			|| decompressedSize != uncompressedSize) {
			throw  std::runtime_error("Decompression failed");
		}

		return decompressedData;
	}
}