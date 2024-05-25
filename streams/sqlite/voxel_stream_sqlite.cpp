#include "voxel_stream_sqlite.h"
#include "../../thirdparty/sqlite/sqlite3.h"
#include "../../util/errors.h"
#include "../../util/godot/classes/project_settings.h"
#include "../../util/godot/core/array.h"
#include "../../util/math/conv.h"
#include "../../util/math/funcs.h"
#include "../../util/profiling.h"
#include "../../util/string/conv.h"
#include "../../util/string/format.h"
#include "../../util/string/std_string.h"
#include "../compressed_data.h"

#include <limits>
#include <string_view>
#include <unordered_set>

namespace zylann::voxel {

namespace {

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

	static bool validate(const Vector3i pos, uint8_t lod) {
		ZN_ASSERT_RETURN_V(can_convert_to_i16(pos), false);
		ZN_ASSERT_RETURN_V(lod < constants::MAX_LOD, false);
		return true;
	}

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
		b.position.z = math::sign_extend_to_32bit(id & 0x7ffff, 19);
		b.position.y = math::sign_extend_to_32bit((id >> 19) & 0x7ffff, 19);
		b.position.x = math::sign_extend_to_32bit((id >> 38) & 0x7ffff, 19);
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
		ZN_ASSERT_RETURN_V(lod_index < constants::MAX_LOD, false);
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
									  math::sign_extend_to_32bit(xb, 25),
									  math::sign_extend_to_32bit(yb, 25),
									  math::sign_extend_to_32bit(zb, 25)
							  ),
							  lod_index };
	}

	uint64_t encode_u64(VoxelStreamSQLite::CoordinateFormat format) const {
		switch (format) {
			case VoxelStreamSQLite::COORDINATE_FORMAT_U64_X16_Y16_Z16_L16:
				return encode_x16_y16_z16_l16();
			case VoxelStreamSQLite::COORDINATE_FORMAT_U64_X19_Y19_Z19_L7:
				return encode_x19_y19_z19_l7();
			default:
				ZN_CRASH_MSG("Invalid coordinate format");
				return 0;
		}
	}

	static BlockLocation decode_u64(uint64_t id, VoxelStreamSQLite::CoordinateFormat format) {
		switch (format) {
			case VoxelStreamSQLite::COORDINATE_FORMAT_U64_X16_Y16_Z16_L16:
				return decode_x16_y16_z16_l16(id);
			case VoxelStreamSQLite::COORDINATE_FORMAT_U64_X19_Y19_Z19_L7:
				return decode_x19_y19_z19_l7(id);
			default:
				ZN_CRASH_MSG("Invalid coordinate format");
				return BlockLocation();
		}
	}

	static Box3i get_coordinate_range(VoxelStreamSQLite::CoordinateFormat format) {
		switch (format) {
			case VoxelStreamSQLite::COORDINATE_FORMAT_U64_X16_Y16_Z16_L16:
				return Box3i::from_min_max(Vector3iUtil::create(-(1 << 16)), Vector3iUtil::create((1 << 16) - 1));
			case VoxelStreamSQLite::COORDINATE_FORMAT_U64_X19_Y19_Z19_L7:
				return Box3i::from_min_max(Vector3iUtil::create(-(1 << 19)), Vector3iUtil::create((1 << 19) - 1));
			case VoxelStreamSQLite::COORDINATE_FORMAT_STRING_CSD:
				return Box3i::from_min_max(
						Vector3iUtil::create(-constants::MAX_VOLUME_EXTENT),
						Vector3iUtil::create(constants::MAX_VOLUME_EXTENT)
				);
			case VoxelStreamSQLite::COORDINATE_FORMAT_BLOB80_X25_Y25_Z25_L5:
				return Box3i::from_min_max(Vector3iUtil::create(-(1 << 24)), Vector3iUtil::create((1 << 24) - 1));
			default:
				ZN_PRINT_ERROR("Invalid coordinate format");
				return Box3i();
		}
	}

	static uint8_t get_lod_count(VoxelStreamSQLite::CoordinateFormat format) {
		return constants::MAX_LOD;
	}

	inline bool operator==(const BlockLocation &other) const {
		return position == other.position && lod == other.lod;
	}
};

void test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i pos, uint8_t lod_index, std::string_view expected) {
	FixedArray<uint8_t, STRING_LOCATION_MAX_LENGTH> buffer;
	const BlockLocation loc{ pos, lod_index };
	std::string_view sv = loc.encode_string_csd(buffer);
	ZN_ASSERT(sv == expected);
	BlockLocation loc2;
	ZN_ASSERT(loc.decode_string_csd(sv, loc2));
	ZN_ASSERT(loc == loc2);
}

void test_voxel_stream_sqlite_key_string_csd_encoding() {
	test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i(0, 0, 0), 0, "0,0,0,0");
	test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i(1, 0, 0), 1, "1,0,0,1");
	test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i(-1, 4, -1), 2, "-1,4,-1,2");
	test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i(6, -9, 21), 5, "6,-9,21,5");
	test_voxel_stream_sqlite_key_string_csd_encoding(Vector3i(123, -456, 789), 20, "123,-456,789,20");
}

void test_voxel_stream_sqlite_key_blob80_encoding(Vector3i position, uint8_t lod_index) {
	FixedArray<uint8_t, BLOB80_LENGTH> buffer;
	const BlockLocation loc{ position, lod_index };
	loc.encode_blob80(to_span(buffer));
	const BlockLocation loc2 = BlockLocation::decode_blob80(to_span(buffer));
	ZN_ASSERT(loc == loc2);
}

void test_voxel_stream_sqlite_key_blob80_encoding() {
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(0, 0, 0), 0);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(1, 0, 0), 1);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(-1, 4, -1), 2);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(6, -9, 21), 5);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(123, -456, 789), 20);

	const VoxelStreamSQLite::CoordinateFormat format = VoxelStreamSQLite::COORDINATE_FORMAT_BLOB80_X25_Y25_Z25_L5;
	const Box3i limits = BlockLocation::get_coordinate_range(format);
	const uint8_t max_lod_index = BlockLocation::get_lod_count(format) - 1;
	const Vector3i min_pos = limits.position;
	const Vector3i max_pos = limits.position + limits.size - Vector3i(1, 1, 1);
	test_voxel_stream_sqlite_key_blob80_encoding(min_pos, max_lod_index);
	test_voxel_stream_sqlite_key_blob80_encoding(max_pos, max_lod_index);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(min_pos.x, max_pos.y, min_pos.z), max_lod_index);
	test_voxel_stream_sqlite_key_blob80_encoding(Vector3i(max_pos.x, min_pos.y, max_pos.z), max_lod_index);
}

} // namespace

void test_sqlite_stream_utility_functions() {
	test_voxel_stream_sqlite_key_string_csd_encoding();
	test_voxel_stream_sqlite_key_blob80_encoding();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// One connection to the database, with our prepared statements
class VoxelStreamSQLiteInternal {
public:
	static const int VERSION_V0 = 0;
	static const int VERSION_V1 = 1;
	static const int VERSION_LATEST = VERSION_V1;

	struct Meta {
		int version = -1;
		int block_size_po2 = 0;
		VoxelStreamSQLite::CoordinateFormat coordinate_format =
				// Default as of V0
				VoxelStreamSQLite::COORDINATE_FORMAT_U64_X16_Y16_Z16_L16;

		struct Channel {
			VoxelBuffer::Depth depth;
			bool used = false;
		};

		FixedArray<Channel, VoxelBuffer::MAX_CHANNELS> channels;
	};

	enum BlockType { //
		VOXELS,
		INSTANCES
	};

	VoxelStreamSQLiteInternal();
	~VoxelStreamSQLiteInternal();

	bool open(const char *fpath, const VoxelStreamSQLite::CoordinateFormat preferred_coordinate_format);
	void close();

	bool is_open() const {
		return _db != nullptr;
	}

	// Returns the file path from SQLite
	const char *get_file_path() const;

	// Return the file path that was used to open the connection.
	// You may use this one if you want determinism, as SQLite seems to globalize its path.
	const char *get_opened_file_path() const {
		return _opened_path.c_str();
	}

	bool begin_transaction();
	bool end_transaction();

	bool save_block(const BlockLocation loc, const Span<const uint8_t> block_data, const BlockType type);

	VoxelStream::ResultCode load_block(
			const BlockLocation loc,
			StdVector<uint8_t> &out_block_data,
			const BlockType type
	);

	bool load_all_blocks(
			void *callback_data,
			void (*process_block_func)(
					void *callback_data,
					BlockLocation location,
					Span<const uint8_t> voxel_data,
					Span<const uint8_t> instances_data
			)
	);

	bool load_all_block_keys(
			void *callback_data,
			void (*process_block_func)(void *callback_data, BlockLocation location)
	);

	const Meta &get_meta() const {
		return _meta;
	}

	void migrate_to_latest_version();

private:
	int load_version();
	Meta load_meta();
	void save_meta(Meta meta);
	bool migrate_to_next_version();
	bool migrate_from_v0_to_v1();

	enum CoordinateColumnType {
		COORDINATE_COLUMN_U64,
		COORDINATE_COLUMN_STRING,
		COORDINATE_COLUMN_BLOB,
	};

	static inline CoordinateColumnType get_coordinate_column_type(VoxelStreamSQLite::CoordinateFormat cf) {
		switch (cf) {
			case VoxelStreamSQLite::COORDINATE_FORMAT_U64_X16_Y16_Z16_L16:
			case VoxelStreamSQLite::COORDINATE_FORMAT_U64_X19_Y19_Z19_L7:
				return COORDINATE_COLUMN_U64;
			case VoxelStreamSQLite::COORDINATE_FORMAT_STRING_CSD:
				return COORDINATE_COLUMN_STRING;
			case VoxelStreamSQLite::COORDINATE_FORMAT_BLOB80_X25_Y25_Z25_L5:
				return COORDINATE_COLUMN_BLOB;
			default:
				ZN_CRASH_MSG("Invalid coordinate format");
				return COORDINATE_COLUMN_U64;
		}
	}

	struct BindBlockCoordinates {
		BlockLocationBuffer buffer;
		CoordinateColumnType key_column_type;

		inline bool bind(
				sqlite3 *db,
				sqlite3_stmt *statement,
				int param_index,
				const VoxelStreamSQLite::CoordinateFormat coordinate_format,
				const BlockLocation location
		) {
			key_column_type = get_coordinate_column_type(coordinate_format);

			switch (key_column_type) {
				case COORDINATE_COLUMN_U64: {
					const uint64_t eloc = location.encode_u64(coordinate_format);

					const int rc = sqlite3_bind_int64(statement, param_index, eloc);
					if (rc != SQLITE_OK) {
						ERR_PRINT(sqlite3_errmsg(db));
						return false;
					}
				} break;

				case COORDINATE_COLUMN_STRING: {
					const std::string_view eloc = location.encode_string_csd(buffer);

					// We use SQLITE_STATIC to tell SQLite we are managing that memory
					const int rc = sqlite3_bind_text(statement, param_index, eloc.data(), eloc.size(), SQLITE_STATIC);
					if (rc != SQLITE_OK) {
						ERR_PRINT(sqlite3_errmsg(db));
						return false;
					}
				} break;

				case COORDINATE_COLUMN_BLOB: {
					Span<uint8_t> eloc = to_span(buffer).sub(0, BLOB80_LENGTH);
					location.encode_blob80(eloc);

					// We use SQLITE_STATIC to tell SQLite we are managing that memory
					const int rc = sqlite3_bind_blob(statement, param_index, eloc.data(), eloc.size(), SQLITE_STATIC);
					if (rc != SQLITE_OK) {
						ERR_PRINT(sqlite3_errmsg(db));
						return false;
					}
				} break;

				default:
					ZN_CRASH_MSG("Invalid coordinate format");
					return false;
			}

			return true;
		}

		inline bool unbind(sqlite3 *db, sqlite3_stmt *statement, int param_index) {
			// Not done at the moment. We assume the statement will always be reset before use, and bindings will ALWAYS
			// be overwritten the next time a query gets made.
			// The code below currently throws an `SQLITE_MISUSE` error because we would also need to call reset() after
			// the query...

			// Unbind the key for correctness, as the doc says the use of SQLITE_STATIC means the bound object must
			// remain valid until the parameter is bound to something else (in which case, we explicitely do it).
			// Not sure if we actually need to do that in practice.
			//
			// if (key_column_type == COORDINATE_COLUMN_STRING) {
			// 	const int rc = sqlite3_bind_text(statement, param_index, nullptr, 0, SQLITE_STATIC);
			// 	if (rc != SQLITE_DONE) {
			// 		ERR_PRINT(sqlite3_errmsg(db));
			// 		return false;
			// 	}
			// } else if (key_column_type == COORDINATE_COLUMN_BLOB) {
			// 	const int rc = sqlite3_bind_blob(statement, param_index, nullptr, 0, SQLITE_STATIC);
			// 	if (rc != SQLITE_DONE) {
			// 		ERR_PRINT(sqlite3_errmsg(db));
			// 		return false;
			// 	}
			// }
			return true;
		}
	};

	inline bool read_block_location(
			const CoordinateColumnType key_column_type,
			sqlite3_stmt *statement,
			const int param_index,
			BlockLocation &out_location
	) {
		switch (key_column_type) {
			case COORDINATE_COLUMN_U64: {
				const uint64_t eloc = sqlite3_column_int64(statement, param_index);
				out_location = BlockLocation::decode_u64(eloc, _meta.coordinate_format);
			} break;

			case COORDINATE_COLUMN_STRING: {
				const unsigned char *eloc = sqlite3_column_text(statement, param_index);
				const unsigned int eloc_len = sqlite3_column_bytes(statement, param_index);
				// That's ugly...
				const std::string_view s(reinterpret_cast<const char *>(eloc), eloc_len);
				ZN_ASSERT_RETURN_V(BlockLocation::decode_string_csd(s, out_location), false);
			} break;

			case COORDINATE_COLUMN_BLOB: {
				const void *eloc = sqlite3_column_blob(statement, param_index);
				const unsigned int eloc_len = sqlite3_column_bytes(statement, param_index);
				Span<const uint8_t> s(static_cast<const uint8_t *>(eloc), eloc_len);
				out_location = BlockLocation::decode_blob80(s);
			} break;

			default:
				ZN_CRASH_MSG("Invalid coordinate column type");
				break;
		}

		return true;
	}

	struct TransactionScope {
		VoxelStreamSQLiteInternal &db;
		TransactionScope(VoxelStreamSQLiteInternal &p_db) : db(p_db) {
			db.begin_transaction();
		}
		~TransactionScope() {
			db.end_transaction();
		}
	};

	static bool prepare(sqlite3 *db, sqlite3_stmt **s, const char *sql) {
		const int rc = sqlite3_prepare_v2(db, sql, -1, s, nullptr);
		if (rc != SQLITE_OK) {
			ERR_PRINT(String("Preparing statement failed: {0}").format(varray(sqlite3_errmsg(db))));
			return false;
		}
		return true;
	}

	static void finalize(sqlite3_stmt *&s) {
		if (s != nullptr) {
			sqlite3_finalize(s);
			s = nullptr;
		}
	}

	StdString _opened_path;
	Meta _meta;
	sqlite3 *_db = nullptr;
	sqlite3_stmt *_load_version_statement = nullptr;
	sqlite3_stmt *_begin_statement = nullptr;
	sqlite3_stmt *_end_statement = nullptr;
	sqlite3_stmt *_update_voxel_block_statement = nullptr;
	sqlite3_stmt *_get_voxel_block_statement = nullptr;
	sqlite3_stmt *_update_instance_block_statement = nullptr;
	sqlite3_stmt *_get_instance_block_statement = nullptr;
	sqlite3_stmt *_load_meta_statement = nullptr;
	sqlite3_stmt *_save_meta_statement = nullptr;
	sqlite3_stmt *_load_channels_statement = nullptr;
	sqlite3_stmt *_save_channel_statement = nullptr;
	sqlite3_stmt *_load_all_blocks_statement = nullptr;
	sqlite3_stmt *_load_all_block_keys_statement = nullptr;
};

VoxelStreamSQLiteInternal::VoxelStreamSQLiteInternal() {}

VoxelStreamSQLiteInternal::~VoxelStreamSQLiteInternal() {
	close();
}

bool VoxelStreamSQLiteInternal::open(
		const char *fpath,
		const VoxelStreamSQLite::CoordinateFormat preferred_coordinate_format
) {
	ZN_PROFILE_SCOPE();
	close();

	int rc = sqlite3_open_v2(fpath, &_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (rc != 0) {
		ZN_PRINT_ERROR(format("Could not open database at path \"{}\": {}", fpath, sqlite3_errmsg(_db)));
		close();
		return false;
	}

	// Note, SQLite uses UTF-8 encoding by default. We rely on that.
	// https://www.sqlite.org/c3ref/open.html

	sqlite3 *db = _db;
	char *error_message = nullptr;

	const CoordinateColumnType block_key_column_type = get_coordinate_column_type(preferred_coordinate_format);

	// Create tables if they don't exist.
	const char *tables[3] = {
		"CREATE TABLE IF NOT EXISTS meta (version INTEGER, block_size_po2 INTEGER, coordinate_format INTEGER)",
		"",
		"CREATE TABLE IF NOT EXISTS channels (idx INTEGER PRIMARY KEY, depth INTEGER)"
	};
	switch (block_key_column_type) {
		case COORDINATE_COLUMN_U64:
			tables[1] = "CREATE TABLE IF NOT EXISTS blocks (loc INTEGER PRIMARY KEY, vb BLOB, instances BLOB)";
			break;
		case COORDINATE_COLUMN_STRING:
			tables[1] = "CREATE TABLE IF NOT EXISTS blocks (loc TEXT PRIMARY KEY, vb BLOB, instances BLOB)";
			break;
		case COORDINATE_COLUMN_BLOB:
			tables[1] = "CREATE TABLE IF NOT EXISTS blocks (loc BLOB PRIMARY KEY, vb BLOB, instances BLOB)";
			break;
		default:
			ZN_CRASH_MSG("Invalid column type");
			break;
	}
	for (size_t i = 0; i < 3; ++i) {
		rc = sqlite3_exec(db, tables[i], nullptr, nullptr, &error_message);
		if (rc != SQLITE_OK) {
			ERR_PRINT(String("Failed to create table: {0}").format(varray(error_message)));
			sqlite3_free(error_message);
			close();
			return false;
		}
	}

	if (!prepare(db, &_load_version_statement, "SELECT version FROM meta")) {
		return false;
	}

	const int version = load_version();
	if (version == -1) {
		return false;
	}

	// Prepare statements
	if (!prepare(
				db,
				&_update_voxel_block_statement,
				"INSERT INTO blocks VALUES (:loc, :vb, null) "
				"ON CONFLICT(loc) DO UPDATE SET vb=excluded.vb"
		)) {
		return false;
	}
	if (!prepare(db, &_get_voxel_block_statement, "SELECT vb FROM blocks WHERE loc=:loc")) {
		return false;
	}
	if (!prepare(
				db,
				&_update_instance_block_statement,
				"INSERT INTO blocks VALUES (:loc, null, :instances) "
				"ON CONFLICT(loc) DO UPDATE SET instances=excluded.instances"
		)) {
		return false;
	}
	if (!prepare(db, &_get_instance_block_statement, "SELECT instances FROM blocks WHERE loc=:loc")) {
		return false;
	}
	if (!prepare(db, &_begin_statement, "BEGIN")) {
		return false;
	}
	if (!prepare(db, &_end_statement, "END")) {
		return false;
	}
	if (!prepare(db, &_load_meta_statement, "SELECT * FROM meta")) {
		return false;
	}

	if (version == VERSION_V0) {
		if (!prepare(db, &_save_meta_statement, "INSERT INTO meta VALUES (:version, :block_size_po2)")) {
			return false;
		}
	} else if (version == VERSION_LATEST) {
		if (!prepare(
					db, &_save_meta_statement, "INSERT INTO meta VALUES (:version, :block_size_po2, :coordinate_format)"
			)) {
			return false;
		}
	} else {
		ZN_PRINT_ERROR(format("Invalid version: {}", version));
		return false;
	}

	if (!prepare(db, &_load_channels_statement, "SELECT * FROM channels")) {
		return false;
	}
	if (!prepare(
				db,
				&_save_channel_statement,
				"INSERT INTO channels VALUES (:idx, :depth) "
				"ON CONFLICT(idx) DO UPDATE SET depth=excluded.depth"
		)) {
		return false;
	}
	if (!prepare(db, &_load_all_blocks_statement, "SELECT * FROM blocks")) {
		return false;
	}
	if (!prepare(db, &_load_all_block_keys_statement, "SELECT loc FROM blocks")) {
		return false;
	}

	// Is the database setup?
	Meta meta = load_meta();
	if (meta.version == -1) {
		// Setup database
		meta.version = VERSION_LATEST;
		// Defaults
		meta.block_size_po2 = constants::DEFAULT_BLOCK_SIZE_PO2;
		meta.coordinate_format = preferred_coordinate_format;
		for (unsigned int i = 0; i < meta.channels.size(); ++i) {
			Meta::Channel &channel = meta.channels[i];
			channel.used = true;
			channel.depth = VoxelBuffer::DEPTH_16_BIT;
		}
		save_meta(meta);
	} else {
		if (meta.coordinate_format != preferred_coordinate_format) {
			ZN_PRINT_VERBOSE(
					format("Opened database uses version {} (latest is {}) and uses coordinate format {} while the "
						   "preferred format is {}.",
						   meta.version,
						   VERSION_LATEST,
						   meta.coordinate_format,
						   preferred_coordinate_format)
			);
		}
	}

	_meta = meta;
	_opened_path = fpath;
	return true;
}

void VoxelStreamSQLiteInternal::close() {
	if (_db == nullptr) {
		return;
	}
	finalize(_begin_statement);
	finalize(_end_statement);
	finalize(_load_version_statement);
	finalize(_update_voxel_block_statement);
	finalize(_get_voxel_block_statement);
	finalize(_update_instance_block_statement);
	finalize(_get_instance_block_statement);
	finalize(_load_meta_statement);
	finalize(_save_meta_statement);
	finalize(_load_channels_statement);
	finalize(_save_channel_statement);
	finalize(_load_all_blocks_statement);
	finalize(_load_all_block_keys_statement);
	sqlite3_close(_db);
	_db = nullptr;
	_opened_path.clear();
}

const char *VoxelStreamSQLiteInternal::get_file_path() const {
	if (_db == nullptr) {
		return nullptr;
	}
	return sqlite3_db_filename(_db, nullptr);
}

bool VoxelStreamSQLiteInternal::begin_transaction() {
	int rc = sqlite3_reset(_begin_statement);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(_db));
		return false;
	}
	rc = sqlite3_step(_begin_statement);
	if (rc != SQLITE_DONE) {
		ERR_PRINT(sqlite3_errmsg(_db));
		return false;
	}
	return true;
}

bool VoxelStreamSQLiteInternal::end_transaction() {
	int rc = sqlite3_reset(_end_statement);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(_db));
		return false;
	}
	rc = sqlite3_step(_end_statement);
	if (rc != SQLITE_DONE) {
		ERR_PRINT(sqlite3_errmsg(_db));
		return false;
	}
	return true;
}

bool VoxelStreamSQLiteInternal::save_block(
		const BlockLocation loc,
		const Span<const uint8_t> block_data,
		const BlockType type
) {
	ZN_PROFILE_SCOPE();

	sqlite3 *db = _db;

	sqlite3_stmt *update_block_statement;
	switch (type) {
		case VOXELS:
			update_block_statement = _update_voxel_block_statement;
			break;
		case INSTANCES:
			update_block_statement = _update_instance_block_statement;
			break;
		default:
			CRASH_NOW();
	}

	int rc = sqlite3_reset(update_block_statement);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
		return false;
	}

	BindBlockCoordinates block_coordinates_binding;
	if (!block_coordinates_binding.bind(db, update_block_statement, 1, _meta.coordinate_format, loc)) {
		return false;
	}

	if (block_data.size() == 0) {
		rc = sqlite3_bind_null(update_block_statement, 2);
	} else {
		// We use SQLITE_TRANSIENT so SQLite will make its own copy of the data
		rc = sqlite3_bind_blob(update_block_statement, 2, block_data.data(), block_data.size(), SQLITE_TRANSIENT);
	}
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
		return false;
	}

	rc = sqlite3_step(update_block_statement);
	if (rc != SQLITE_DONE) {
		ERR_PRINT(sqlite3_errmsg(db));
		return false;
	}

	if (!block_coordinates_binding.unbind(db, update_block_statement, 1)) {
		return false;
	}

	return true;
}

VoxelStream::ResultCode VoxelStreamSQLiteInternal::load_block(
		const BlockLocation loc,
		StdVector<uint8_t> &out_block_data,
		const BlockType type
) {
	sqlite3 *db = _db;

	sqlite3_stmt *get_block_statement;
	switch (type) {
		case VOXELS:
			get_block_statement = _get_voxel_block_statement;
			break;
		case INSTANCES:
			get_block_statement = _get_instance_block_statement;
			break;
		default:
			CRASH_NOW();
	}

	int rc;

	rc = sqlite3_reset(get_block_statement);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
		return VoxelStream::RESULT_ERROR;
	}

	BindBlockCoordinates block_coordinates_binding;
	if (!block_coordinates_binding.bind(db, get_block_statement, 1, _meta.coordinate_format, loc)) {
		return VoxelStream::RESULT_ERROR;
	}

	VoxelStream::ResultCode result = VoxelStream::RESULT_BLOCK_NOT_FOUND;

	while (true) {
		rc = sqlite3_step(get_block_statement);
		if (rc == SQLITE_ROW) {
			const void *blob = sqlite3_column_blob(get_block_statement, 0);
			// const uint8_t *b = reinterpret_cast<const uint8_t *>(blob);
			const size_t blob_size = sqlite3_column_bytes(get_block_statement, 0);
			if (blob_size != 0) {
				result = VoxelStream::RESULT_BLOCK_FOUND;
				out_block_data.resize(blob_size);
				memcpy(out_block_data.data(), blob, blob_size);
			}
			// The query is still ongoing, we'll need to step one more time to complete it
			continue;
		}
		if (rc != SQLITE_DONE) {
			ERR_PRINT(sqlite3_errmsg(db));
			return VoxelStream::RESULT_ERROR;
		}
		break;
	}

	ZN_ASSERT_RETURN_V(block_coordinates_binding.unbind(db, get_block_statement, 1), VoxelStream::RESULT_ERROR);

	return result;
}

bool VoxelStreamSQLiteInternal::load_all_blocks(
		void *callback_data,
		void (*process_block_func)(
				void *callback_data,
				BlockLocation location,
				Span<const uint8_t> voxel_data,
				Span<const uint8_t> instances_data
		)
) {
	ZN_PROFILE_SCOPE();
	CRASH_COND(process_block_func == nullptr);

	sqlite3 *db = _db;
	sqlite3_stmt *load_all_blocks_statement = _load_all_blocks_statement;

	int rc;

	rc = sqlite3_reset(load_all_blocks_statement);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
		return false;
	}

	const CoordinateColumnType key_column_type = get_coordinate_column_type(_meta.coordinate_format);

	while (true) {
		rc = sqlite3_step(load_all_blocks_statement);

		if (rc == SQLITE_ROW) {
			ZN_PROFILE_SCOPE_NAMED("Row");

			BlockLocation loc;
			ZN_ASSERT_CONTINUE(read_block_location(key_column_type, load_all_blocks_statement, 0, loc));

			const void *voxels_blob = sqlite3_column_blob(load_all_blocks_statement, 1);
			const size_t voxels_blob_size = sqlite3_column_bytes(load_all_blocks_statement, 1);

			const void *instances_blob = sqlite3_column_blob(load_all_blocks_statement, 2);
			const size_t instances_blob_size = sqlite3_column_bytes(load_all_blocks_statement, 2);

			// Using a function pointer because returning a big list of a copy of all the blobs can
			// waste a lot of temporary memory
			process_block_func(
					callback_data,
					loc,
					Span<const uint8_t>(reinterpret_cast<const uint8_t *>(voxels_blob), voxels_blob_size),
					Span<const uint8_t>(reinterpret_cast<const uint8_t *>(instances_blob), instances_blob_size)
			);

		} else if (rc == SQLITE_DONE) {
			break;

		} else {
			ERR_PRINT(String("Unexpected SQLite return code: {0}; errmsg: {1}").format(rc, sqlite3_errmsg(db)));
			return false;
		}
	}

	return true;
}

bool VoxelStreamSQLiteInternal::load_all_block_keys(
		void *callback_data,
		void (*process_block_func)(void *callback_data, BlockLocation location)
) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(process_block_func != nullptr);

	sqlite3 *db = _db;
	sqlite3_stmt *load_all_block_keys_statement = _load_all_block_keys_statement;

	int rc;

	rc = sqlite3_reset(load_all_block_keys_statement);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
		return false;
	}

	const CoordinateColumnType key_column_type = get_coordinate_column_type(_meta.coordinate_format);

	while (true) {
		rc = sqlite3_step(load_all_block_keys_statement);

		if (rc == SQLITE_ROW) {
			ZN_PROFILE_SCOPE_NAMED("Row");

			BlockLocation loc;
			ZN_ASSERT_CONTINUE(read_block_location(key_column_type, load_all_block_keys_statement, 0, loc));

			// Using a function pointer because returning a big list of a copy of all the blobs can
			// waste a lot of temporary memory
			process_block_func(callback_data, loc);

		} else if (rc == SQLITE_DONE) {
			break;

		} else {
			ERR_PRINT(String("Unexpected SQLite return code: {0}; errmsg: {1}").format(rc, sqlite3_errmsg(db)));
			return false;
		}
	}

	return true;
}

int VoxelStreamSQLiteInternal::load_version() {
	sqlite3 *db = _db;
	sqlite3_stmt *load_version_statement = _load_version_statement;

	int rc = sqlite3_reset(load_version_statement);
	if (rc != SQLITE_OK) {
		ZN_PRINT_ERROR(sqlite3_errmsg(db));
		return -1;
	}

	int version = -1;

	rc = sqlite3_step(load_version_statement);
	if (rc == SQLITE_ROW) {
		version = sqlite3_column_int(load_version_statement, 0);
		// The query is still ongoing, we'll need to step one more time to complete it
		rc = sqlite3_step(load_version_statement);
	} else {
		// There was no row. This database is probably not setup.
		return VERSION_LATEST;
	}

	if (rc != SQLITE_DONE) {
		ZN_PRINT_ERROR(sqlite3_errmsg(db));
		return -1;
	}

	return version;
}

VoxelStreamSQLiteInternal::Meta VoxelStreamSQLiteInternal::load_meta() {
	sqlite3 *db = _db;
	sqlite3_stmt *load_meta_statement = _load_meta_statement;
	sqlite3_stmt *load_channels_statement = _load_channels_statement;

	int rc = sqlite3_reset(load_meta_statement);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
		return Meta();
	}

	TransactionScope transaction(*this);

	Meta meta;
	bool invalid_version = false;

	rc = sqlite3_step(load_meta_statement);
	if (rc == SQLITE_ROW) {
		meta.version = sqlite3_column_int(load_meta_statement, 0);
		meta.block_size_po2 = sqlite3_column_int(load_meta_statement, 1);

		if (meta.version == VERSION_V0) {
			meta.coordinate_format = VoxelStreamSQLite::COORDINATE_FORMAT_U64_X16_Y16_Z16_L16;
		} else if (meta.version == VERSION_LATEST) {
			meta.coordinate_format =
					static_cast<VoxelStreamSQLite::CoordinateFormat>(sqlite3_column_int(load_meta_statement, 2));
		} else {
			invalid_version = true;
		}

		// The query is still ongoing, we'll need to step one more time to complete it
		rc = sqlite3_step(load_meta_statement);

	} else if (rc == SQLITE_DONE) {
		// There was no row. This database is probably not setup.
		return Meta();
	}
	if (rc != SQLITE_DONE) {
		ERR_PRINT(sqlite3_errmsg(db));
		return Meta();
	}

	ZN_ASSERT_RETURN_V(!invalid_version, Meta());

	ZN_ASSERT_RETURN_V_MSG(
			meta.coordinate_format >= 0 && meta.coordinate_format < VoxelStreamSQLite::COORDINATE_FORMAT_COUNT,
			Meta(),
			format("Invalid coordinate format: {}", meta.coordinate_format)
	);

	rc = sqlite3_reset(load_channels_statement);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
		return Meta();
	}

	while (true) {
		rc = sqlite3_step(load_channels_statement);
		if (rc == SQLITE_ROW) {
			const int index = sqlite3_column_int(load_channels_statement, 0);
			const int depth = sqlite3_column_int(load_channels_statement, 1);
			if (index < 0 || index >= static_cast<int>(meta.channels.size())) {
				ERR_PRINT(String("Channel index {0} is invalid").format(varray(index)));
				continue;
			}
			if (depth < 0 || depth >= VoxelBuffer::DEPTH_COUNT) {
				ERR_PRINT(String("Depth {0} is invalid").format(varray(depth)));
				continue;
			}
			Meta::Channel &channel = meta.channels[index];
			channel.used = true;
			channel.depth = static_cast<VoxelBuffer::Depth>(depth);
			continue;
		}
		if (rc != SQLITE_DONE) {
			ERR_PRINT(sqlite3_errmsg(db));
			return Meta();
		}
		break;
	}

	return meta;
}

void VoxelStreamSQLiteInternal::save_meta(Meta meta) {
	sqlite3 *db = _db;
	sqlite3_stmt *save_meta_statement = _save_meta_statement;
	sqlite3_stmt *save_channel_statement = _save_channel_statement;

	int rc = sqlite3_reset(save_meta_statement);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
		return;
	}

	TransactionScope transaction(*this);

	rc = sqlite3_bind_int(save_meta_statement, 1, meta.version);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
		return;
	}
	rc = sqlite3_bind_int(save_meta_statement, 2, meta.block_size_po2);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
		return;
	}
	if (meta.version == VERSION_LATEST) {
		rc = sqlite3_bind_int(save_meta_statement, 3, meta.coordinate_format);
		if (rc != SQLITE_OK) {
			ZN_PRINT_ERROR(sqlite3_errmsg(db));
			return;
		}
	}

	rc = sqlite3_step(save_meta_statement);
	if (rc != SQLITE_DONE) {
		ERR_PRINT(sqlite3_errmsg(db));
		return;
	}

	for (unsigned int channel_index = 0; channel_index < meta.channels.size(); ++channel_index) {
		const Meta::Channel &channel = meta.channels[channel_index];
		if (!channel.used) {
			// TODO Remove rows for unused channels? Or have a `used` column?
			continue;
		}

		rc = sqlite3_reset(save_channel_statement);
		if (rc != SQLITE_OK) {
			ERR_PRINT(sqlite3_errmsg(db));
			return;
		}

		rc = sqlite3_bind_int(save_channel_statement, 1, channel_index);
		if (rc != SQLITE_OK) {
			ERR_PRINT(sqlite3_errmsg(db));
			return;
		}
		rc = sqlite3_bind_int(save_channel_statement, 2, channel.depth);
		if (rc != SQLITE_OK) {
			ERR_PRINT(sqlite3_errmsg(db));
			return;
		}

		rc = sqlite3_step(save_channel_statement);
		if (rc != SQLITE_DONE) {
			ERR_PRINT(sqlite3_errmsg(db));
			return;
		}
	}
}

bool VoxelStreamSQLiteInternal::migrate_from_v0_to_v1() {
	if (_meta.version == VERSION_V1) {
		ZN_PRINT_WARNING("Version already matching");
		return true;
	}
	ZN_ASSERT_RETURN_V(_meta.version == VERSION_V0, false);

	// Prepare statements
	struct Statements {
		VoxelStreamSQLiteInternal &db;
		sqlite3_stmt *alter_table = nullptr;
		sqlite3_stmt *update_table = nullptr;

		Statements(VoxelStreamSQLiteInternal &p_db) : db(p_db) {}

		~Statements() {
			db.finalize(alter_table);
			db.finalize(update_table);
		}
	};

	Statements statements(*this);

	ZN_ASSERT_RETURN_V(
			prepare(_db, &statements.alter_table, "ALTER TABLE meta ADD COLUMN coordinate_format INTEGER"), false
	);
	ZN_ASSERT_RETURN_V(prepare(_db, &statements.update_table, "UPDATE meta SET version = :version"), false);

	// Run
	{
		TransactionScope scope(*this);

		int rc = sqlite3_step(statements.alter_table);
		if (rc != SQLITE_DONE) {
			ZN_PRINT_ERROR(sqlite3_errmsg(_db));
			return false;
		}

		rc = sqlite3_bind_int(statements.update_table, 1, VERSION_V1);
		if (rc != SQLITE_OK) {
			ZN_PRINT_ERROR(sqlite3_errmsg(_db));
			return false;
		}

		rc = sqlite3_step(statements.update_table);
		if (rc != SQLITE_DONE) {
			ZN_PRINT_ERROR(sqlite3_errmsg(_db));
			return false;
		}
	}

	_meta.version = VERSION_V1;
	return true;
}

bool VoxelStreamSQLiteInternal::migrate_to_next_version() {
	switch (_meta.version) {
		case VERSION_V0:
			return migrate_from_v0_to_v1();

		case VERSION_LATEST:
			ZN_PRINT_WARNING("Version is already latest");
			break;

		default:
			ZN_PRINT_ERROR(format("Unexpected version: {}", _meta.version));
			return false;
	}

	return true;
}

void VoxelStreamSQLiteInternal::migrate_to_latest_version() {
	ZN_ASSERT_RETURN(is_open());

	while (_meta.version != VERSION_LATEST) {
		const int prev = _meta.version;
		ZN_ASSERT_RETURN(migrate_to_next_version());
		ZN_ASSERT_RETURN(prev != _meta.version);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
StdVector<uint8_t> &get_tls_temp_block_data() {
	thread_local StdVector<uint8_t> tls_temp_block_data;
	return tls_temp_block_data;
}
StdVector<uint8_t> &get_tls_temp_compressed_block_data() {
	thread_local StdVector<uint8_t> tls_temp_compressed_block_data;
	return tls_temp_compressed_block_data;
}
} // namespace

VoxelStreamSQLite::VoxelStreamSQLite() {}

VoxelStreamSQLite::~VoxelStreamSQLite() {
	ZN_PRINT_VERBOSE("~VoxelStreamSQLite");
	if (!_connection_path.is_empty() && _cache.get_indicative_block_count() > 0) {
		ZN_PRINT_VERBOSE("~VoxelStreamSQLite flushy flushy");
		flush_cache();
		ZN_PRINT_VERBOSE("~VoxelStreamSQLite flushy done");
	}
	for (auto it = _connection_pool.begin(); it != _connection_pool.end(); ++it) {
		delete *it;
	}
	_connection_pool.clear();
	ZN_PRINT_VERBOSE("~VoxelStreamSQLite done");
}

void VoxelStreamSQLite::set_database_path(String path) {
	MutexLock lock(_connection_mutex);
	if (path == _connection_path) {
		return;
	}
	if (!_connection_path.is_empty() && _cache.get_indicative_block_count() > 0) {
		// Save cached data before changing the path.
		// Not using get_connection() because it locks, we are already locked.
		VoxelStreamSQLiteInternal con;
		// To support Godot shortcuts like `user://` and `res://` (though the latter won't work on exported builds)
		const String globalized_connection_path = ProjectSettings::get_singleton()->globalize_path(_connection_path);
		const CharString cpath = globalized_connection_path.utf8();
		// Note, the path could be invalid,
		// Since Godot helpfully sets the property for every character typed in the inspector.
		// So there can be lots of errors in the editor if you type it.
		if (con.open(cpath.get_data(), _preferred_coordinate_format)) {
			flush_cache_to_connection(&con);
		}
	}
	for (auto it = _connection_pool.begin(); it != _connection_pool.end(); ++it) {
		delete *it;
	}
	_block_keys_cache.clear();
	_connection_pool.clear();

	_connection_path = path;
	// Don't actually open anything here. We'll do it only when necessary
}

String VoxelStreamSQLite::get_database_path() const {
	MutexLock lock(_connection_mutex);
	return _connection_path;
}

void VoxelStreamSQLite::load_voxel_block(VoxelStream::VoxelQueryData &q) {
	load_voxel_blocks(Span<VoxelStream::VoxelQueryData>(&q, 1));
}

void VoxelStreamSQLite::save_voxel_block(VoxelStream::VoxelQueryData &q) {
	save_voxel_blocks(Span<VoxelStream::VoxelQueryData>(&q, 1));
}

void VoxelStreamSQLite::load_voxel_blocks(Span<VoxelStream::VoxelQueryData> p_blocks) {
	ZN_PROFILE_SCOPE();

	// Getting connection first to allow the key cache to load if enabled.
	// This should be quick after the first call because the connection is cached.
	VoxelStreamSQLiteInternal *con = get_connection();
	ERR_FAIL_COND(con == nullptr);

	// Check the cache first
	StdVector<unsigned int> blocks_to_load;
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		VoxelStream::VoxelQueryData &q = p_blocks[i];
		const Vector3i pos = q.position_in_blocks;

		ZN_ASSERT_CONTINUE(can_convert_to_i16(pos));

		if (_block_keys_cache_enabled && !_block_keys_cache.contains(pos, q.lod_index)) {
			q.result = RESULT_BLOCK_NOT_FOUND;
			continue;
		}

		if (_cache.load_voxel_block(pos, q.lod_index, q.voxel_buffer)) {
			q.result = RESULT_BLOCK_FOUND;

		} else {
			blocks_to_load.push_back(i);
		}
	}

	if (blocks_to_load.size() == 0) {
		// Everything was cached, no need to query the database
		recycle_connection(con);
		return;
	}

	// TODO We should handle busy return codes
	ERR_FAIL_COND(con->begin_transaction() == false);

	for (unsigned int i = 0; i < blocks_to_load.size(); ++i) {
		const unsigned int ri = blocks_to_load[i];
		VoxelStream::VoxelQueryData &q = p_blocks[ri];

		BlockLocation loc;
		loc.position = q.position_in_blocks;
		loc.lod = q.lod_index;

		StdVector<uint8_t> &temp_block_data = get_tls_temp_block_data();

		const ResultCode res = con->load_block(loc, temp_block_data, VoxelStreamSQLiteInternal::VOXELS);

		if (res == RESULT_BLOCK_FOUND) {
			// TODO Not sure if we should actually expect non-null. There can be legit not found blocks.
			BlockSerializer::decompress_and_deserialize(to_span_const(temp_block_data), q.voxel_buffer);
		}

		q.result = res;
	}

	ERR_FAIL_COND(con->end_transaction() == false);

	recycle_connection(con);
}

void VoxelStreamSQLite::save_voxel_blocks(Span<VoxelStream::VoxelQueryData> p_blocks) {
	// First put in cache
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		VoxelStream::VoxelQueryData &q = p_blocks[i];
		const Vector3i pos = q.position_in_blocks;

		if (!BlockLocation::validate(pos, q.lod_index)) {
			ERR_PRINT(String("Block position {0} is outside of supported range").format(varray(pos)));
			continue;
		}

		_cache.save_voxel_block(pos, q.lod_index, q.voxel_buffer);
		if (_block_keys_cache_enabled) {
			_block_keys_cache.add(pos, q.lod_index);
		}
	}

	// TODO We should consider using a serialized cache, and measure the threshold in bytes
	if (_cache.get_indicative_block_count() >= CACHE_SIZE) {
		flush_cache();
	}
}

bool VoxelStreamSQLite::supports_instance_blocks() const {
	return true;
}

void VoxelStreamSQLite::load_instance_blocks(Span<VoxelStream::InstancesQueryData> out_blocks) {
	ZN_PROFILE_SCOPE();

	// TODO Get block size from database
	// const int bs_po2 = constants::DEFAULT_BLOCK_SIZE_PO2;

	// Check the cache first
	StdVector<unsigned int> blocks_to_load;
	for (size_t i = 0; i < out_blocks.size(); ++i) {
		VoxelStream::InstancesQueryData &q = out_blocks[i];

		if (_cache.load_instance_block(q.position_in_blocks, q.lod_index, q.data)) {
			q.result = RESULT_BLOCK_FOUND;

		} else {
			blocks_to_load.push_back(i);
		}
	}

	if (blocks_to_load.size() == 0) {
		// Everything was cached, no need to query the database
		return;
	}

	VoxelStreamSQLiteInternal *con = get_connection();
	ERR_FAIL_COND(con == nullptr);

	// TODO We should handle busy return codes
	// TODO recycle on error
	ERR_FAIL_COND(con->begin_transaction() == false);

	for (unsigned int i = 0; i < blocks_to_load.size(); ++i) {
		const unsigned int ri = blocks_to_load[i];
		VoxelStream::InstancesQueryData &q = out_blocks[ri];

		BlockLocation loc;
		loc.position = q.position_in_blocks;
		loc.lod = q.lod_index;

		StdVector<uint8_t> &temp_compressed_block_data = get_tls_temp_compressed_block_data();

		const ResultCode res = con->load_block(loc, temp_compressed_block_data, VoxelStreamSQLiteInternal::INSTANCES);

		if (res == RESULT_BLOCK_FOUND) {
			StdVector<uint8_t> &temp_block_data = get_tls_temp_block_data();

			if (!CompressedData::decompress(to_span_const(temp_compressed_block_data), temp_block_data)) {
				ERR_PRINT("Failed to decompress instance block");
				q.result = RESULT_ERROR;
				continue;
			}
			q.data = make_unique_instance<InstanceBlockData>();
			if (!deserialize_instance_block_data(*q.data, to_span_const(temp_block_data))) {
				ERR_PRINT("Failed to deserialize instance block");
				q.result = RESULT_ERROR;
				continue;
			}
		}

		q.result = res;
	}

	ERR_FAIL_COND(con->end_transaction() == false);

	recycle_connection(con);
}

void VoxelStreamSQLite::save_instance_blocks(Span<VoxelStream::InstancesQueryData> p_blocks) {
	// TODO Get block size from database
	// const int bs_po2 = constants::DEFAULT_BLOCK_SIZE_PO2;

	// First put in cache
	for (size_t i = 0; i < p_blocks.size(); ++i) {
		VoxelStream::InstancesQueryData &q = p_blocks[i];

		if (!BlockLocation::validate(q.position_in_blocks, q.lod_index)) {
			ZN_PRINT_ERROR(format("Instance block position {} is outside of supported range", q.position_in_blocks));
			continue;
		}

		_cache.save_instance_block(q.position_in_blocks, q.lod_index, std::move(q.data));
		if (_block_keys_cache_enabled) {
			_block_keys_cache.add(q.position_in_blocks, q.lod_index);
		}
	}

	// TODO Optimization: we should consider using a serialized cache, and measure the threshold in bytes
	if (_cache.get_indicative_block_count() >= CACHE_SIZE) {
		flush_cache();
	}
}

void VoxelStreamSQLite::load_all_blocks(FullLoadingResult &result) {
	ZN_PROFILE_SCOPE();

	VoxelStreamSQLiteInternal *con = get_connection();
	ERR_FAIL_COND(con == nullptr);

	struct Context {
		FullLoadingResult &result;
	};

	// Using local function instead of a lambda for quite stupid reason admittedly:
	// Godot's clang-format does not allow to write function parameters in column,
	// which makes the lambda break line length.
	struct L {
		static void process_block_func(
				void *callback_data,
				const BlockLocation location,
				Span<const uint8_t> voxel_data,
				Span<const uint8_t> instances_data
		) {
			Context *ctx = reinterpret_cast<Context *>(callback_data);

			if (voxel_data.size() == 0 && instances_data.size() == 0) {
				ZN_PRINT_VERBOSE(format(
						"Unexpected empty voxel data and instances data at {} lod {}", location.position, location.lod
				));
				return;
			}

			FullLoadingResult::Block result_block;
			result_block.position = location.position;
			result_block.lod = location.lod;

			if (voxel_data.size() > 0) {
				std::shared_ptr<VoxelBuffer> voxels = make_shared_instance<VoxelBuffer>(VoxelBuffer::ALLOCATOR_POOL);
				ERR_FAIL_COND(!BlockSerializer::decompress_and_deserialize(voxel_data, *voxels));
				result_block.voxels = voxels;
			}

			if (instances_data.size() > 0) {
				StdVector<uint8_t> &temp_block_data = get_tls_temp_block_data();
				if (!CompressedData::decompress(instances_data, temp_block_data)) {
					ERR_PRINT("Failed to decompress instance block");
					return;
				}
				result_block.instances_data = make_unique_instance<InstanceBlockData>();
				if (!deserialize_instance_block_data(*result_block.instances_data, to_span_const(temp_block_data))) {
					ERR_PRINT("Failed to deserialize instance block");
					return;
				}
			}

			ctx->result.blocks.push_back(std::move(result_block));
		}
	};

	// Had to suffix `_outer`,
	// because otherwise GCC thinks it shadows a variable inside the local function/captureless lambda
	Context ctx_outer{ result };
	const bool request_result = con->load_all_blocks(&ctx_outer, L::process_block_func);
	ERR_FAIL_COND(request_result == false);
}

int VoxelStreamSQLite::get_used_channels_mask() const {
	// Assuming all, since that stream can store anything.
	return VoxelBuffer::ALL_CHANNELS_MASK;
}

void VoxelStreamSQLite::flush_cache() {
	VoxelStreamSQLiteInternal *con = get_connection();
	ERR_FAIL_COND(con == nullptr);
	flush_cache_to_connection(con);
	recycle_connection(con);
}

void VoxelStreamSQLite::flush() {
	flush_cache();
}

// This function does not lock any mutex for internal use.
void VoxelStreamSQLite::flush_cache_to_connection(VoxelStreamSQLiteInternal *p_connection) {
	ZN_PROFILE_SCOPE();
	ZN_PRINT_VERBOSE(format("VoxelStreamSQLite: Flushing cache ({} elements)", _cache.get_indicative_block_count()));

	ERR_FAIL_COND(p_connection == nullptr);
	ERR_FAIL_COND(p_connection->begin_transaction() == false);

	StdVector<uint8_t> &temp_data = get_tls_temp_block_data();
	StdVector<uint8_t> &temp_compressed_data = get_tls_temp_compressed_block_data();

	// TODO Needs better error rollback handling
	_cache.flush([p_connection, &temp_data, &temp_compressed_data](VoxelStreamCache::Block &block) {
		ERR_FAIL_COND(!BlockLocation::validate(block.position, block.lod));

		BlockLocation loc;
		loc.position = block.position;
		loc.lod = block.lod;

		// Save voxels
		if (block.has_voxels) {
			if (block.voxels_deleted) {
				p_connection->save_block(loc, Span<const uint8_t>(), VoxelStreamSQLiteInternal::VOXELS);
			} else {
				BlockSerializer::SerializeResult res = BlockSerializer::serialize_and_compress(block.voxels);
				ERR_FAIL_COND(!res.success);
				p_connection->save_block(loc, to_span(res.data), VoxelStreamSQLiteInternal::VOXELS);
			}
		}

		// Save instances
		temp_compressed_data.clear();
		if (block.instances != nullptr) {
			temp_data.clear();

			ERR_FAIL_COND(!serialize_instance_block_data(*block.instances, temp_data));

			ERR_FAIL_COND(!CompressedData::compress(
					to_span_const(temp_data), temp_compressed_data, CompressedData::COMPRESSION_NONE
			));
		}
		p_connection->save_block(loc, to_span(temp_compressed_data), VoxelStreamSQLiteInternal::INSTANCES);

		// TODO Optimization: add a version of the query that can update both at once
	});

	ERR_FAIL_COND(p_connection->end_transaction() == false);
}

VoxelStreamSQLiteInternal *VoxelStreamSQLite::get_connection() {
	_connection_mutex.lock();
	if (_connection_path.is_empty()) {
		_connection_mutex.unlock();
		return nullptr;
	}
	if (_connection_pool.size() != 0) {
		VoxelStreamSQLiteInternal *s = _connection_pool.back();
		_connection_pool.pop_back();
		_connection_mutex.unlock();
		return s;
	}
	// First connection we get since we set the database path

	const String fpath = _connection_path;
	const CoordinateFormat preferred_coordinate_format = _preferred_coordinate_format;
	_connection_mutex.unlock();

	if (fpath.is_empty()) {
		return nullptr;
	}
	VoxelStreamSQLiteInternal *con = new VoxelStreamSQLiteInternal();
	// To support Godot shortcuts like `user://` and `res://` (though the latter won't work on exported builds)
	const String globalized_fpath = ProjectSettings::get_singleton()->globalize_path(fpath);
	const CharString fpath_utf8 = globalized_fpath.utf8();
	if (!con->open(fpath_utf8.get_data(), preferred_coordinate_format)) {
		delete con;
		con = nullptr;
	}
	if (_block_keys_cache_enabled) {
		RWLockWrite wlock(_block_keys_cache.rw_lock);
		con->load_all_block_keys(&_block_keys_cache, [](void *ctx, BlockLocation loc) {
			BlockKeysCache *cache = static_cast<BlockKeysCache *>(ctx);
			cache->add_no_lock(loc.position, loc.lod);
		});
	}
	return con;
}

void VoxelStreamSQLite::recycle_connection(VoxelStreamSQLiteInternal *con) {
	String con_path = con->get_opened_file_path();
	_connection_mutex.lock();
	// If path differs, delete this connection
	if (_connection_path != con_path) {
		_connection_mutex.unlock();
		delete con;
	} else {
		_connection_pool.push_back(con);
		_connection_mutex.unlock();
	}
}

void VoxelStreamSQLite::set_key_cache_enabled(bool enable) {
	_block_keys_cache_enabled = enable;
}

bool VoxelStreamSQLite::is_key_cache_enabled() const {
	return _block_keys_cache_enabled;
}

Box3i VoxelStreamSQLite::get_supported_block_range() const {
	// const VoxelStreamSQLiteInternal *con = get_connection();
	// const CoordinateFormat format = con != nullptr ? con->get_meta().coordinate_format :
	// _preferred_coordinate_format;
	const CoordinateFormat format = _preferred_coordinate_format;
	return BlockLocation::get_coordinate_range(format);
}

int VoxelStreamSQLite::get_lod_count() const {
	// const VoxelStreamSQLiteInternal *con = get_connection();
	// const CoordinateFormat format = con != nullptr ? con->get_meta().coordinate_format :
	// _preferred_coordinate_format;
	const CoordinateFormat format = _preferred_coordinate_format;
	return BlockLocation::get_lod_count(format);
}

void VoxelStreamSQLite::set_preferred_coordinate_format(CoordinateFormat format) {
	_preferred_coordinate_format = format;
}

VoxelStreamSQLite::CoordinateFormat VoxelStreamSQLite::get_preferred_coordinate_format() const {
	return _preferred_coordinate_format;
}

VoxelStreamSQLite::CoordinateFormat VoxelStreamSQLite::get_current_coordinate_format() {
	const VoxelStreamSQLiteInternal *con = get_connection();
	if (con == nullptr) {
		return get_preferred_coordinate_format();
	}
	return con->get_meta().coordinate_format;
}

void VoxelStreamSQLite::copy_blocks_to_other_sqlite_stream(Ref<VoxelStreamSQLite> dst_stream) {
	// This function may be used as a generic way to migrate an old save to a new one, when the format of the old one
	// needs to change. If it's just a version change, it might be possible to do it in-place, however changes like
	// coordinate format affect primary keys, so not doing it in-place is easier.

	ZN_ASSERT_RETURN(dst_stream.is_valid());
	ZN_ASSERT_RETURN(dst_stream.ptr() != this);

	VoxelStreamSQLiteInternal *src_con = get_connection();
	ZN_ASSERT_RETURN(src_con != nullptr);

	ZN_ASSERT_RETURN(dst_stream->get_database_path() != get_database_path());

	// We can skip deserialization and copy data blocks directly.
	// We also don't use cache.

	struct Context {
		VoxelStreamSQLiteInternal *dst_con;

		static void save(
				void *cb_data,
				BlockLocation location,
				Span<const uint8_t> voxel_data,
				Span<const uint8_t> instances_data
		) {
			Context *ctx = static_cast<Context *>(cb_data);
			ctx->dst_con->save_block(location, voxel_data, VoxelStreamSQLiteInternal::VOXELS);
			ctx->dst_con->save_block(location, instances_data, VoxelStreamSQLiteInternal::INSTANCES);
		}
	};

	Context context;
	context.dst_con = dst_stream->get_connection();

	src_con->load_all_blocks(&context, Context::save);
}

void VoxelStreamSQLite::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database_path", "path"), &VoxelStreamSQLite::set_database_path);
	ClassDB::bind_method(D_METHOD("get_database_path"), &VoxelStreamSQLite::get_database_path);

	ClassDB::bind_method(D_METHOD("set_key_cache_enabled", "enabled"), &VoxelStreamSQLite::set_key_cache_enabled);
	ClassDB::bind_method(D_METHOD("is_key_cache_enabled"), &VoxelStreamSQLite::is_key_cache_enabled);

	ADD_PROPERTY(
			PropertyInfo(Variant::STRING, "database_path", PROPERTY_HINT_FILE), "set_database_path", "get_database_path"
	);
}

} // namespace zylann::voxel
