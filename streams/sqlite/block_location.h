#ifndef VOXEL_STREAM_SQLITE_BLOCK_LOCATION_H
#define VOXEL_STREAM_SQLITE_BLOCK_LOCATION_H

#include "../../constants/voxel_constants.h"
#include "../../util/containers/fixed_array.h"
#include "../../util/math/box3i.h"
#include "../../util/math/vector3i.h"
#include "../../util/string/conv.h"
#include <limits>

namespace zylann::voxel::sqlite {

// x,y,z,lod where lod in [0..24[
static constexpr unsigned int STRING_LOCATION_MAX_LENGTH = MAX_INT32_CHAR_COUNT_BASE10 * 3 + 3 + 2;
static constexpr unsigned int BLOB80_LENGTH = 10;
static constexpr unsigned int LOCATION_BUFFER_MAX_LENGTH = math::max(STRING_LOCATION_MAX_LENGTH, BLOB80_LENGTH);
using BlockLocationBuffer = FixedArray<uint8_t, LOCATION_BUFFER_MAX_LENGTH>;

inline constexpr uint32_t bits_u32(unsigned int nbits) {
	return (1 << nbits) - 1;
}

struct BlockLocation {
	Vector3i position;
	uint8_t lod;

	enum CoordinateFormat {
		// Blocks: -32,768..32,767
		// Voxels: -524,288..524,287
		// LODs: 24
		FORMAT_INT64_X16_Y16_Z16_L16 = 0,
		// Blocks: -262,144..262,143
		// Voxels: -4,194,304..4,194,303
		// LODs: 24
		FORMAT_INT64_X19_Y19_Z19_L7,
		// Full range, but might be slowest
		FORMAT_STRING_CSD,
		// Blocks: -16,777,216..16,777,215
		// Voxels: -268,435,456..268,435,455
		// LODs: 24
		FORMAT_BLOB80_X25_Y25_Z25_L5,
		FORMAT_COUNT,
	};

	uint64_t encode_x16_y16_z16_l16() const {
		// 0l xx yy zz
		// TODO Is that actually correct with negative coordinates?
		return ((static_cast<uint64_t>(lod) & 0xffff) << 48) | ((static_cast<uint64_t>(position.x) & 0xffff) << 32) |
				((static_cast<uint64_t>(position.y) & 0xffff) << 16) | (static_cast<uint64_t>(position.z) & 0xffff);
	}

	static BlockLocation decode_x16_y16_z16_l16(uint64_t id) {
		BlockLocation b;
		// We cast first to restore the sign
		b.position.z = static_cast<int16_t>(id & 0xffff);
		b.position.y = static_cast<int16_t>((id >> 16) & 0xffff);
		b.position.x = static_cast<int16_t>((id >> 32) & 0xffff);
		b.lod = ((id >> 48) & 0xff);
		return b;
	}

	uint64_t encode_x19_y19_z19_l7() const {
		// lllllllx xxxxxxxx xxxxxxxx xxyyyyyy yyyyyyyy yyyyyzzz zzzzzzzz zzzzzzzz
		return ((static_cast<uint64_t>(lod) & 0x7f) << 57) | ((static_cast<uint64_t>(position.x) & 0x7ffff) << 38) |
				((static_cast<uint64_t>(position.y) & 0x7ffff) << 19) | (static_cast<uint64_t>(position.z) & 0x7ffff);
	}

	static BlockLocation decode_x19_y19_z19_l7(uint64_t id) {
		BlockLocation b;
		b.position.z = math::sign_extend_to_32bit<19>(id & 0x7ffff);
		b.position.y = math::sign_extend_to_32bit<19>((id >> 19) & 0x7ffff);
		b.position.x = math::sign_extend_to_32bit<19>((id >> 38) & 0x7ffff);
		b.lod = ((id >> 57) & 0x7f);
		return b;
	}

	std::string_view encode_string_csd(BlockLocationBuffer &buffer) const {
		Span<uint8_t> s = to_span(buffer);
		unsigned int pos = int32_to_string_base10(position.x, s);
		s[pos] = ',';
		++pos;
		pos += int32_to_string_base10(position.y, s.sub(pos));
		s[pos] = ',';
		++pos;
		pos += int32_to_string_base10(position.z, s.sub(pos));
		s[pos] = ',';
		++pos;
		pos += int32_to_string_base10(lod, s.sub(pos));
		// s[pos] = '\0';
		// ++pos;
		return std::string_view(reinterpret_cast<const char *>(buffer.data()), pos);
	}

	static bool decode_string_csd(std::string_view s, BlockLocation &out_location) {
		BlockLocation location;

		int res = string_base10_to_int32(s, location.position.x);
		ZN_ASSERT_RETURN_V(res > 0, false);
		unsigned int pos = res;
		ZN_ASSERT_RETURN_V(s[pos] == ',', false);
		++pos;

		res = string_base10_to_int32(s.substr(pos), location.position.y);
		ZN_ASSERT_RETURN_V(res > 0, false);
		pos += res;
		ZN_ASSERT_RETURN_V(s[pos] == ',', false);
		++pos;

		res = string_base10_to_int32(s.substr(pos), location.position.z);
		ZN_ASSERT_RETURN_V(res > 0, false);
		pos += res;
		ZN_ASSERT_RETURN_V(s[pos] == ',', false);
		++pos;

		int32_t lod_index;
		res = string_base10_to_int32(s.substr(pos), lod_index);
		ZN_ASSERT_RETURN_V(res > 0, false);
		pos += res;
		ZN_ASSERT_RETURN_V(lod_index >= 0, false);
		ZN_ASSERT_RETURN_V(lod_index < static_cast<int32_t>(constants::MAX_LOD), false);
		location.lod = lod_index;

		out_location = location;
		return true;
	}

	void encode_blob80(Span<uint8_t> dst) const {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(dst.size() == BLOB80_LENGTH);
#endif
		const uint32_t xb = static_cast<uint32_t>(position.x);
		const uint32_t yb = static_cast<uint32_t>(position.y);
		const uint32_t zb = static_cast<uint32_t>(position.z);
		const uint32_t lb = lod;
		// Byte |   9        8        7        6        5        4        3        2        1        0
		// -----|--------|--------|--------|--------|--------|--------|--------|--------|--------|--------
		// Bits |lllllzzz zzzzzzzz zzzzzzzz zzzzzzyy yyyyyyyy yyyyyyyy yyyyyyyx xxxxxxxx xxxxxxxx xxxxxxxx
		dst[0] = (xb >> 0) & bits_u32(8); // x[0..7]
		dst[1] = (xb >> 8) & bits_u32(8); // x[8..15]
		dst[2] = (xb >> 16) & bits_u32(8); // x[16..23]
		dst[3] = ((xb >> 24) & bits_u32(1)) | ((yb & bits_u32(7)) << 1); // x[24] | y[0..6]
		dst[4] = (yb >> 7) & bits_u32(8); // y[7..14]
		dst[5] = (yb >> 15) & bits_u32(8); // y[15..22]
		dst[6] = ((yb >> 23) & bits_u32(2)) | ((zb & bits_u32(6)) << 2); // y[23..24] | z[0..5]
		dst[7] = (zb >> 6) & bits_u32(8); // y[6..13]
		dst[8] = (zb >> 14) & bits_u32(8); // y[14..21]
		dst[9] = ((zb >> 22) & bits_u32(3)) | ((lb & bits_u32(5)) << 3); // y[22..24] | l[0..4]
	}

	static BlockLocation decode_blob80(Span<const uint8_t> src) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(src.size() == BLOB80_LENGTH);
#endif
		const uint32_t xb = //
				static_cast<uint32_t>(src[0]) | //
				(static_cast<uint32_t>(src[1]) << 8) | //
				(static_cast<uint32_t>(src[2]) << 16) | //
				(static_cast<uint32_t>(src[3] & 1) << 24);
		const uint32_t yb = //
				static_cast<uint32_t>(src[3] >> 1) | //
				(static_cast<uint32_t>(src[4]) << 7) | //
				(static_cast<uint32_t>(src[5]) << 15) | //
				(static_cast<uint32_t>(src[6] & 0b11) << 23);
		const uint32_t zb = //
				static_cast<uint32_t>(src[6] >> 2) | //
				(static_cast<uint32_t>(src[7]) << 6) | //
				(static_cast<uint32_t>(src[8]) << 14) | //
				(static_cast<uint32_t>(src[9] & 0b111) << 22);
		const uint8_t lod_index = src[9] >> 3;
		return BlockLocation{ Vector3i(
									  math::sign_extend_to_32bit<25>(xb),
									  math::sign_extend_to_32bit<25>(yb),
									  math::sign_extend_to_32bit<25>(zb)
							  ),
							  lod_index };
	}

	uint64_t encode_u64(BlockLocation::CoordinateFormat format) const {
		switch (format) {
			case FORMAT_INT64_X16_Y16_Z16_L16:
				return encode_x16_y16_z16_l16();
			case FORMAT_INT64_X19_Y19_Z19_L7:
				return encode_x19_y19_z19_l7();
			default:
				ZN_CRASH_MSG("Invalid coordinate format");
				return 0;
		}
	}

	static BlockLocation decode_u64(uint64_t id, BlockLocation::CoordinateFormat format) {
		switch (format) {
			case FORMAT_INT64_X16_Y16_Z16_L16:
				return decode_x16_y16_z16_l16(id);
			case FORMAT_INT64_X19_Y19_Z19_L7:
				return decode_x19_y19_z19_l7(id);
			default:
				ZN_CRASH_MSG("Invalid coordinate format");
				return BlockLocation();
		}
	}

	static Box3i get_coordinate_range(BlockLocation::CoordinateFormat format) {
		switch (format) {
			case FORMAT_INT64_X16_Y16_Z16_L16:
				return Box3i::from_min_max(Vector3iUtil::create(-(1 << 15)), Vector3iUtil::create((1 << 15) - 1));
			case FORMAT_INT64_X19_Y19_Z19_L7:
				return Box3i::from_min_max(Vector3iUtil::create(-(1 << 18)), Vector3iUtil::create((1 << 18) - 1));
			case FORMAT_STRING_CSD:
				// In theory should be maximum an int32 can hold, but let's use the maximum extent we can get with the
				// module's own limit constant
				return Box3i::from_min_max(
						Vector3iUtil::create(-(constants::MAX_VOLUME_EXTENT >> constants::DEFAULT_BLOCK_SIZE_PO2)),
						Vector3iUtil::create((constants::MAX_VOLUME_EXTENT >> constants::DEFAULT_BLOCK_SIZE_PO2))
				);
			case FORMAT_BLOB80_X25_Y25_Z25_L5:
				return Box3i::from_min_max(Vector3iUtil::create(-(1 << 24)), Vector3iUtil::create((1 << 24) - 1));
			default:
				ZN_PRINT_ERROR("Invalid coordinate format");
				return Box3i();
		}
	}

	static uint8_t get_lod_count(BlockLocation::CoordinateFormat format) {
		return constants::MAX_LOD;
	}

	inline bool operator==(const BlockLocation &other) const {
		return position == other.position && lod == other.lod;
	}
};

} // namespace zylann::voxel::sqlite

#endif // VOXEL_STREAM_SQLITE_BLOCK_LOCATION_H
