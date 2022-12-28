#include "voxel_stream_sqlite.h"
#include "../../thirdparty/sqlite/sqlite3.h"
#include "../../util/errors.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/funcs.h"
#include "../../util/log.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "../compressed_data.h"

#include <limits>
#include <string>
#include <unordered_set>

namespace zylann::voxel {

struct BlockLocation {
	int16_t x;
	int16_t y;
	int16_t z;
	uint8_t lod;

	static bool validate(const Vector3i pos, uint8_t lod) {
		ZN_ASSERT_RETURN_V(can_convert_to_i16(pos), false);
		ZN_ASSERT_RETURN_V(lod < constants::MAX_LOD, false);
		return true;
	}

	uint64_t encode() const {
		// 0l xx yy zz
		// TODO Is this valid with negative numbers?
		return ((static_cast<uint64_t>(lod) & 0xffff) << 48) | ((static_cast<uint64_t>(x) & 0xffff) << 32) |
				((static_cast<uint64_t>(y) & 0xffff) << 16) | (static_cast<uint64_t>(z) & 0xffff);
	}

	static BlockLocation decode(uint64_t id) {
		BlockLocation b;
		b.z = (id & 0xffff);
		b.y = ((id >> 16) & 0xffff);
		b.x = ((id >> 32) & 0xffff);
		b.lod = ((id >> 48) & 0xff);
		return b;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// One connection to the database, with our prepared statements
class VoxelStreamSQLiteInternal {
public:
	static const int VERSION = 0;

	struct Meta {
		int version = -1;
		int block_size_po2 = 0;

		struct Channel {
			VoxelBufferInternal::Depth depth;
			bool used = false;
		};

		FixedArray<Channel, VoxelBufferInternal::MAX_CHANNELS> channels;
	};

	enum BlockType { //
		VOXELS,
		INSTANCES
	};

	VoxelStreamSQLiteInternal();
	~VoxelStreamSQLiteInternal();

	bool open(const char *fpath);
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

	bool save_block(BlockLocation loc, const std::vector<uint8_t> &block_data, BlockType type);
	VoxelStream::ResultCode load_block(BlockLocation loc, std::vector<uint8_t> &out_block_data, BlockType type);

	bool load_all_blocks(void *callback_data,
			void (*process_block_func)(void *callback_data, BlockLocation location, Span<const uint8_t> voxel_data,
					Span<const uint8_t> instances_data));

	bool load_all_block_keys(
			void *callback_data, void (*process_block_func)(void *callback_data, BlockLocation location));

	Meta load_meta();
	void save_meta(Meta meta);

private:
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

	std::string _opened_path;
	sqlite3 *_db = nullptr;
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

bool VoxelStreamSQLiteInternal::open(const char *fpath) {
	ZN_PROFILE_SCOPE();
	close();

	int rc = sqlite3_open_v2(fpath, &_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if (rc != 0) {
		ERR_PRINT(String("Could not open database: {0}").format(varray(sqlite3_errmsg(_db))));
		close();
		return false;
	}

	sqlite3 *db = _db;
	char *error_message = nullptr;

	// Create tables if they dont exist
	const char *tables[3] = { "CREATE TABLE IF NOT EXISTS meta (version INTEGER, block_size_po2 INTEGER)",
		"CREATE TABLE IF NOT EXISTS blocks (loc INTEGER PRIMARY KEY, vb BLOB, instances BLOB)",
		"CREATE TABLE IF NOT EXISTS channels (idx INTEGER PRIMARY KEY, depth INTEGER)" };
	for (size_t i = 0; i < 3; ++i) {
		rc = sqlite3_exec(db, tables[i], nullptr, nullptr, &error_message);
		if (rc != SQLITE_OK) {
			ERR_PRINT(String("Failed to create table: {0}").format(varray(error_message)));
			sqlite3_free(error_message);
			close();
			return false;
		}
	}

	// Prepare statements
	if (!prepare(db, &_update_voxel_block_statement,
				"INSERT INTO blocks VALUES (:loc, :vb, null) "
				"ON CONFLICT(loc) DO UPDATE SET vb=excluded.vb")) {
		return false;
	}
	if (!prepare(db, &_get_voxel_block_statement, "SELECT vb FROM blocks WHERE loc=:loc")) {
		return false;
	}
	if (!prepare(db, &_update_instance_block_statement,
				"INSERT INTO blocks VALUES (:loc, null, :instances) "
				"ON CONFLICT(loc) DO UPDATE SET instances=excluded.instances")) {
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
	if (!prepare(db, &_save_meta_statement, "INSERT INTO meta VALUES (:version, :block_size_po2)")) {
		return false;
	}
	if (!prepare(db, &_load_channels_statement, "SELECT * FROM channels")) {
		return false;
	}
	if (!prepare(db, &_save_channel_statement,
				"INSERT INTO channels VALUES (:idx, :depth) "
				"ON CONFLICT(idx) DO UPDATE SET depth=excluded.depth")) {
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
		meta.version = VERSION;
		// Defaults
		meta.block_size_po2 = constants::DEFAULT_BLOCK_SIZE_PO2;
		for (unsigned int i = 0; i < meta.channels.size(); ++i) {
			Meta::Channel &channel = meta.channels[i];
			channel.used = true;
			channel.depth = VoxelBufferInternal::DEPTH_16_BIT;
		}
		save_meta(meta);
	}

	_opened_path = fpath;
	return true;
}

void VoxelStreamSQLiteInternal::close() {
	if (_db == nullptr) {
		return;
	}
	finalize(_begin_statement);
	finalize(_end_statement);
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

bool VoxelStreamSQLiteInternal::save_block(BlockLocation loc, const std::vector<uint8_t> &block_data, BlockType type) {
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

	const uint64_t eloc = loc.encode();

	rc = sqlite3_bind_int64(update_block_statement, 1, eloc);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
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

	return true;
}

VoxelStream::ResultCode VoxelStreamSQLiteInternal::load_block(
		BlockLocation loc, std::vector<uint8_t> &out_block_data, BlockType type) {
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

	const uint64_t eloc = loc.encode();

	rc = sqlite3_bind_int64(get_block_statement, 1, eloc);
	if (rc != SQLITE_OK) {
		ERR_PRINT(sqlite3_errmsg(db));
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

	return result;
}

bool VoxelStreamSQLiteInternal::load_all_blocks(void *callback_data,
		void (*process_block_func)(void *callback_data, BlockLocation location, Span<const uint8_t> voxel_data,
				Span<const uint8_t> instances_data)) {
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

	while (true) {
		rc = sqlite3_step(load_all_blocks_statement);

		if (rc == SQLITE_ROW) {
			ZN_PROFILE_SCOPE_NAMED("Row");

			const uint64_t eloc = sqlite3_column_int64(load_all_blocks_statement, 0);
			const BlockLocation loc = BlockLocation::decode(eloc);

			const void *voxels_blob = sqlite3_column_blob(load_all_blocks_statement, 1);
			const size_t voxels_blob_size = sqlite3_column_bytes(load_all_blocks_statement, 1);

			const void *instances_blob = sqlite3_column_blob(load_all_blocks_statement, 2);
			const size_t instances_blob_size = sqlite3_column_bytes(load_all_blocks_statement, 2);

			// Using a function pointer because returning a big list of a copy of all the blobs can
			// waste a lot of temporary memory
			process_block_func(callback_data, loc,
					Span<const uint8_t>(reinterpret_cast<const uint8_t *>(voxels_blob), voxels_blob_size),
					Span<const uint8_t>(reinterpret_cast<const uint8_t *>(instances_blob), instances_blob_size));

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
		void *callback_data, void (*process_block_func)(void *callback_data, BlockLocation location)) {
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

	while (true) {
		rc = sqlite3_step(load_all_block_keys_statement);

		if (rc == SQLITE_ROW) {
			ZN_PROFILE_SCOPE_NAMED("Row");

			const uint64_t eloc = sqlite3_column_int64(load_all_block_keys_statement, 0);
			const BlockLocation loc = BlockLocation::decode(eloc);

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

	rc = sqlite3_step(load_meta_statement);
	if (rc == SQLITE_ROW) {
		meta.version = sqlite3_column_int(load_meta_statement, 0);
		meta.block_size_po2 = sqlite3_column_int(load_meta_statement, 1);
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
			if (depth < 0 || depth >= VoxelBufferInternal::DEPTH_COUNT) {
				ERR_PRINT(String("Depth {0} is invalid").format(varray(depth)));
				continue;
			}
			Meta::Channel &channel = meta.channels[index];
			channel.used = true;
			channel.depth = static_cast<VoxelBufferInternal::Depth>(depth);
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
std::vector<uint8_t> &get_tls_temp_block_data() {
	thread_local std::vector<uint8_t> tls_temp_block_data;
	return tls_temp_block_data;
}
std::vector<uint8_t> &get_tls_temp_compressed_block_data() {
	thread_local std::vector<uint8_t> tls_temp_compressed_block_data;
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
		// Not using get_connection() because it locks.
		VoxelStreamSQLiteInternal con;
		const CharString cpath = path.utf8();
		// Note, the path could be invalid,
		// Since Godot helpfully sets the property for every character typed in the inspector.
		// So there can be lots of errors in the editor if you type it.
		if (con.open(cpath.get_data())) {
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

	// TODO Get block size from database
	const int bs_po2 = constants::DEFAULT_BLOCK_SIZE_PO2;

	// Check the cache first
	std::vector<unsigned int> blocks_to_load;
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		VoxelStream::VoxelQueryData &q = p_blocks[i];
		const Vector3i pos = q.origin_in_voxels >> (bs_po2 + q.lod);

		ZN_ASSERT_CONTINUE(can_convert_to_i16(pos));

		if (_block_keys_cache_enabled && !_block_keys_cache.contains(to_vec3i16(pos), q.lod)) {
			q.result = RESULT_BLOCK_NOT_FOUND;
			continue;
		}

		if (_cache.load_voxel_block(pos, q.lod, q.voxel_buffer)) {
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
	ERR_FAIL_COND(con->begin_transaction() == false);

	for (unsigned int i = 0; i < blocks_to_load.size(); ++i) {
		const unsigned int ri = blocks_to_load[i];
		VoxelStream::VoxelQueryData &q = p_blocks[ri];

		const unsigned int po2 = bs_po2 + q.lod;

		BlockLocation loc;
		loc.x = q.origin_in_voxels.x >> po2;
		loc.y = q.origin_in_voxels.y >> po2;
		loc.z = q.origin_in_voxels.z >> po2;
		loc.lod = q.lod;

		std::vector<uint8_t> &temp_block_data = get_tls_temp_block_data();

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
	// TODO Get block size from database
	const int bs_po2 = constants::DEFAULT_BLOCK_SIZE_PO2;

	// First put in cache
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		VoxelStream::VoxelQueryData &q = p_blocks[i];
		const Vector3i pos = q.origin_in_voxels >> (bs_po2 + q.lod);

		if (!BlockLocation::validate(pos, q.lod)) {
			ERR_PRINT(String("Block position {0} is outside of supported range").format(varray(pos)));
			continue;
		}

		_cache.save_voxel_block(pos, q.lod, q.voxel_buffer);
		if (_block_keys_cache_enabled) {
			_block_keys_cache.add(to_vec3i16(pos), q.lod);
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
	std::vector<unsigned int> blocks_to_load;
	for (size_t i = 0; i < out_blocks.size(); ++i) {
		VoxelStream::InstancesQueryData &q = out_blocks[i];

		if (_cache.load_instance_block(q.position, q.lod, q.data)) {
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
		loc.x = q.position.x;
		loc.y = q.position.y;
		loc.z = q.position.z;
		loc.lod = q.lod;

		std::vector<uint8_t> &temp_compressed_block_data = get_tls_temp_compressed_block_data();

		const ResultCode res = con->load_block(loc, temp_compressed_block_data, VoxelStreamSQLiteInternal::INSTANCES);

		if (res == RESULT_BLOCK_FOUND) {
			std::vector<uint8_t> &temp_block_data = get_tls_temp_block_data();

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

		if (!BlockLocation::validate(q.position, q.lod)) {
			ZN_PRINT_ERROR(format("Instance block position {} is outside of supported range", q.position));
			continue;
		}

		_cache.save_instance_block(q.position, q.lod, std::move(q.data));
		if (_block_keys_cache_enabled) {
			_block_keys_cache.add(to_vec3i16(q.position), q.lod);
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
		static void process_block_func(void *callback_data, const BlockLocation location,
				Span<const uint8_t> voxel_data, Span<const uint8_t> instances_data) {
			Context *ctx = reinterpret_cast<Context *>(callback_data);

			if (voxel_data.size() == 0 && instances_data.size() == 0) {
				ZN_PRINT_VERBOSE(format("Unexpected empty voxel data and instances data at {} lod {}",
						Vector3i(location.x, location.y, location.z), location.lod));
				return;
			}

			FullLoadingResult::Block result_block;
			result_block.position = Vector3i(location.x, location.y, location.z);
			result_block.lod = location.lod;

			if (voxel_data.size() > 0) {
				std::shared_ptr<VoxelBufferInternal> voxels = make_shared_instance<VoxelBufferInternal>();
				ERR_FAIL_COND(!BlockSerializer::decompress_and_deserialize(voxel_data, *voxels));
				result_block.voxels = voxels;
			}

			if (instances_data.size() > 0) {
				std::vector<uint8_t> &temp_block_data = get_tls_temp_block_data();
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
	return VoxelBufferInternal::ALL_CHANNELS_MASK;
}

void VoxelStreamSQLite::flush_cache() {
	VoxelStreamSQLiteInternal *con = get_connection();
	ERR_FAIL_COND(con == nullptr);
	flush_cache_to_connection(con);
	recycle_connection(con);
}

// This function does not lock any mutex for internal use.
void VoxelStreamSQLite::flush_cache_to_connection(VoxelStreamSQLiteInternal *p_connection) {
	ZN_PROFILE_SCOPE();
	ZN_PRINT_VERBOSE(format("VoxelStreamSQLite: Flushing cache ({} elements)", _cache.get_indicative_block_count()));

	ERR_FAIL_COND(p_connection == nullptr);
	ERR_FAIL_COND(p_connection->begin_transaction() == false);

	std::vector<uint8_t> &temp_data = get_tls_temp_block_data();
	std::vector<uint8_t> &temp_compressed_data = get_tls_temp_compressed_block_data();

	// TODO Needs better error rollback handling
	_cache.flush([p_connection, &temp_data, &temp_compressed_data](VoxelStreamCache::Block &block) {
		ERR_FAIL_COND(!BlockLocation::validate(block.position, block.lod));

		BlockLocation loc;
		loc.x = block.position.x;
		loc.y = block.position.y;
		loc.z = block.position.z;
		loc.lod = block.lod;

		// Save voxels
		if (block.has_voxels) {
			if (block.voxels_deleted) {
				const std::vector<uint8_t> empty;
				p_connection->save_block(loc, empty, VoxelStreamSQLiteInternal::VOXELS);
			} else {
				BlockSerializer::SerializeResult res = BlockSerializer::serialize_and_compress(block.voxels);
				ERR_FAIL_COND(!res.success);
				p_connection->save_block(loc, res.data, VoxelStreamSQLiteInternal::VOXELS);
			}
		}

		// Save instances
		temp_compressed_data.clear();
		if (block.instances != nullptr) {
			temp_data.clear();

			ERR_FAIL_COND(!serialize_instance_block_data(*block.instances, temp_data));

			ERR_FAIL_COND(!CompressedData::compress(
					to_span_const(temp_data), temp_compressed_data, CompressedData::COMPRESSION_NONE));
		}
		p_connection->save_block(loc, temp_compressed_data, VoxelStreamSQLiteInternal::INSTANCES);

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

	String fpath = _connection_path;
	_connection_mutex.unlock();

	if (fpath.is_empty()) {
		return nullptr;
	}
	VoxelStreamSQLiteInternal *con = new VoxelStreamSQLiteInternal();
	const CharString fpath_utf8 = fpath.utf8();
	if (!con->open(fpath_utf8.get_data())) {
		delete con;
		con = nullptr;
	}
	if (_block_keys_cache_enabled) {
		RWLockWrite wlock(_block_keys_cache.rw_lock);
		con->load_all_block_keys(&_block_keys_cache, [](void *ctx, BlockLocation loc) {
			BlockKeysCache *cache = static_cast<BlockKeysCache *>(ctx);
			cache->add_no_lock({ loc.x, loc.y, loc.z }, loc.lod);
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

void VoxelStreamSQLite::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database_path", "path"), &VoxelStreamSQLite::set_database_path);
	ClassDB::bind_method(D_METHOD("get_database_path"), &VoxelStreamSQLite::get_database_path);

	ClassDB::bind_method(D_METHOD("set_key_cache_enabled", "enabled"), &VoxelStreamSQLite::set_key_cache_enabled);
	ClassDB::bind_method(D_METHOD("is_key_cache_enabled"), &VoxelStreamSQLite::is_key_cache_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "database_path", PROPERTY_HINT_FILE), "set_database_path",
			"get_database_path");
}

} // namespace zylann::voxel
