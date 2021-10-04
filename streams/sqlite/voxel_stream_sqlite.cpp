#include "voxel_stream_sqlite.h"
#include "../../thirdparty/sqlite/sqlite3.h"
#include "../../util/macros.h"
#include "../../util/profiling.h"
#include "../compressed_data.h"
#include <limits>
#include <string>

struct BlockLocation {
	int16_t x;
	int16_t y;
	int16_t z;
	uint8_t lod;

	static bool validate(const Vector3i pos, uint8_t lod) {
		ERR_FAIL_COND_V(pos.x < std::numeric_limits<int16_t>::min(), false);
		ERR_FAIL_COND_V(pos.y < std::numeric_limits<int16_t>::min(), false);
		ERR_FAIL_COND_V(pos.z < std::numeric_limits<int16_t>::min(), false);
		ERR_FAIL_COND_V(pos.x > std::numeric_limits<int16_t>::max(), false);
		ERR_FAIL_COND_V(pos.y > std::numeric_limits<int16_t>::max(), false);
		ERR_FAIL_COND_V(pos.z > std::numeric_limits<int16_t>::max(), false);
		return true;
	}

	uint64_t encode() const {
		// 0l xx yy zz
		return ((static_cast<uint64_t>(lod) & 0xffff) << 48) |
			   ((static_cast<uint64_t>(x) & 0xffff) << 32) |
			   ((static_cast<uint64_t>(y) & 0xffff) << 16) |
			   (static_cast<uint64_t>(z) & 0xffff);
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
			VoxelBuffer::Depth depth;
			bool used = false;
		};

		FixedArray<Channel, VoxelBuffer::MAX_CHANNELS> channels;
	};

	enum BlockType {
		VOXELS,
		INSTANCES
	};

	VoxelStreamSQLiteInternal();
	~VoxelStreamSQLiteInternal();

	bool open(const char *fpath);
	void close();

	bool is_open() const { return _db != nullptr; }

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
	VoxelStream::Result load_block(BlockLocation loc, std::vector<uint8_t> &out_block_data, BlockType type);

	Meta load_meta();
	void save_meta(Meta meta);

private:
	struct TransactionScope {
		VoxelStreamSQLiteInternal &db;
		TransactionScope(VoxelStreamSQLiteInternal &p_db) :
				db(p_db) {
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
};

VoxelStreamSQLiteInternal::VoxelStreamSQLiteInternal() {
}

VoxelStreamSQLiteInternal::~VoxelStreamSQLiteInternal() {
	close();
}

bool VoxelStreamSQLiteInternal::open(const char *fpath) {
	VOXEL_PROFILE_SCOPE();
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
	const char *tables[3] = {
		"CREATE TABLE IF NOT EXISTS meta (version INTEGER, block_size_po2 INTEGER)",
		"CREATE TABLE IF NOT EXISTS blocks (loc INTEGER PRIMARY KEY, vb BLOB, instances BLOB)",
		"CREATE TABLE IF NOT EXISTS channels (idx INTEGER PRIMARY KEY, depth INTEGER)"
	};
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

	// Is the database setup?
	Meta meta = load_meta();
	if (meta.version == -1) {
		// Setup database
		meta.version = VERSION;
		// Defaults
		meta.block_size_po2 = VoxelConstants::DEFAULT_BLOCK_SIZE_PO2;
		for (unsigned int i = 0; i < meta.channels.size(); ++i) {
			Meta::Channel &channel = meta.channels[i];
			channel.used = true;
			channel.depth = VoxelBuffer::DEPTH_16_BIT;
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
	VOXEL_PROFILE_SCOPE();

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

VoxelStream::Result VoxelStreamSQLiteInternal::load_block(
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

	VoxelStream::Result result = VoxelStream::RESULT_BLOCK_NOT_FOUND;

	while (true) {
		rc = sqlite3_step(get_block_statement);
		if (rc == SQLITE_ROW) {
			const void *blob = sqlite3_column_blob(get_block_statement, 0);
			//const uint8_t *b = reinterpret_cast<const uint8_t *>(blob);
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

thread_local VoxelBlockSerializerInternal VoxelStreamSQLite::_voxel_block_serializer;
thread_local std::vector<uint8_t> VoxelStreamSQLite::_temp_block_data;
thread_local std::vector<uint8_t> VoxelStreamSQLite::_temp_compressed_block_data;

VoxelStreamSQLite::VoxelStreamSQLite() {
}

VoxelStreamSQLite::~VoxelStreamSQLite() {
	PRINT_VERBOSE("~VoxelStreamSQLite");
	if (!_connection_path.empty() && _cache.get_indicative_block_count() > 0) {
		PRINT_VERBOSE("~VoxelStreamSQLite flushy flushy");
		flush_cache();
		PRINT_VERBOSE("~VoxelStreamSQLite flushy done");
	}
	for (auto it = _connection_pool.begin(); it != _connection_pool.end(); ++it) {
		delete *it;
	}
	_connection_pool.clear();
	PRINT_VERBOSE("~VoxelStreamSQLite done");
}

void VoxelStreamSQLite::set_database_path(String path) {
	MutexLock lock(_connection_mutex);
	if (path == _connection_path) {
		return;
	}
	if (!_connection_path.empty() && _cache.get_indicative_block_count() > 0) {
		// Save cached data before changing the path.
		// Not using get_connection() because it locks.
		VoxelStreamSQLiteInternal con;
		CharString cpath = path.utf8();
		// Note, the path could be invalid,
		// Since Godot helpfully sets the property for every character typed in the inspector.
		// So there can be lots of errors in the editor if you type it.
		if (con.open(cpath)) {
			flush_cache(&con);
		}
	}
	for (auto it = _connection_pool.begin(); it != _connection_pool.end(); ++it) {
		delete *it;
	}
	_connection_pool.clear();
	_connection_path = path;
	// Don't actually open anything here. We'll do it only when necessary
}

String VoxelStreamSQLite::get_database_path() const {
	MutexLock lock(_connection_mutex);
	return _connection_path;
}

VoxelStream::Result VoxelStreamSQLite::emerge_block(VoxelBufferInternal &out_buffer, Vector3i origin_in_voxels, int lod) {
	VoxelBlockRequest r{ out_buffer, origin_in_voxels, lod };
	Vector<Result> results;
	emerge_blocks(Span<VoxelBlockRequest>(&r, 1), results);
	CRASH_COND(results.size() != 1);
	return results[0];
}

void VoxelStreamSQLite::immerge_block(VoxelBufferInternal &buffer, Vector3i origin_in_voxels, int lod) {
	VoxelBlockRequest r{ buffer, origin_in_voxels, lod };
	immerge_blocks(Span<VoxelBlockRequest>(&r, 1));
}

void VoxelStreamSQLite::emerge_blocks(Span<VoxelBlockRequest> p_blocks, Vector<Result> &out_results) {
	VOXEL_PROFILE_SCOPE();

	// TODO Get block size from database
	const int bs_po2 = VoxelConstants::DEFAULT_BLOCK_SIZE_PO2;

	out_results.resize(p_blocks.size());

	// Check the cache first
	Vector<int> blocks_to_load;
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		VoxelBlockRequest &wr = p_blocks[i];
		const Vector3i pos = wr.origin_in_voxels >> (bs_po2 + wr.lod);

		Ref<VoxelBuffer> vb;
		if (_cache.load_voxel_block(pos, wr.lod, wr.voxel_buffer)) {
			out_results.write[i] = RESULT_BLOCK_FOUND;

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

	for (int i = 0; i < blocks_to_load.size(); ++i) {
		const int ri = blocks_to_load[i];
		const VoxelBlockRequest &r = p_blocks[ri];

		const unsigned int po2 = bs_po2 + r.lod;

		BlockLocation loc;
		loc.x = r.origin_in_voxels.x >> po2;
		loc.y = r.origin_in_voxels.y >> po2;
		loc.z = r.origin_in_voxels.z >> po2;
		loc.lod = r.lod;

		const Result res = con->load_block(loc, _temp_block_data, VoxelStreamSQLiteInternal::VOXELS);

		if (res == RESULT_BLOCK_FOUND) {
			VoxelBlockRequest &wr = p_blocks[ri];
			// TODO Not sure if we should actually expect non-null. There can be legit not found blocks.
			_voxel_block_serializer.decompress_and_deserialize(to_span_const(_temp_block_data), wr.voxel_buffer);
		}

		out_results.write[i] = res;
	}

	ERR_FAIL_COND(con->end_transaction() == false);

	recycle_connection(con);
}

void VoxelStreamSQLite::immerge_blocks(Span<VoxelBlockRequest> p_blocks) {
	// TODO Get block size from database
	const int bs_po2 = VoxelConstants::DEFAULT_BLOCK_SIZE_PO2;

	// First put in cache
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		VoxelBlockRequest &r = p_blocks[i];
		const Vector3i pos = r.origin_in_voxels >> (bs_po2 + r.lod);

		if (!BlockLocation::validate(pos, r.lod)) {
			ERR_PRINT(String("Block position {0} is outside of supported range").format(varray(pos.to_vec3())));
			continue;
		}

		_cache.save_voxel_block(pos, r.lod, r.voxel_buffer);
	}

	// TODO We should consider using a serialized cache, and measure the threshold in bytes
	if (_cache.get_indicative_block_count() >= CACHE_SIZE) {
		flush_cache();
	}
}

bool VoxelStreamSQLite::supports_instance_blocks() const {
	return true;
}

void VoxelStreamSQLite::load_instance_blocks(
		Span<VoxelStreamInstanceDataRequest> out_blocks, Span<Result> out_results) {
	VOXEL_PROFILE_SCOPE();

	// TODO Get block size from database
	//const int bs_po2 = VoxelConstants::DEFAULT_BLOCK_SIZE_PO2;

	// Check the cache first
	Vector<int> blocks_to_load;
	for (size_t i = 0; i < out_blocks.size(); ++i) {
		VoxelStreamInstanceDataRequest &r = out_blocks[i];

		if (_cache.load_instance_block(r.position, r.lod, r.data)) {
			out_results[i] = RESULT_BLOCK_FOUND;

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

	for (int i = 0; i < blocks_to_load.size(); ++i) {
		const int ri = blocks_to_load[i];
		VoxelStreamInstanceDataRequest &r = out_blocks[ri];

		BlockLocation loc;
		loc.x = r.position.x;
		loc.y = r.position.y;
		loc.z = r.position.z;
		loc.lod = r.lod;

		const Result res = con->load_block(loc, _temp_compressed_block_data, VoxelStreamSQLiteInternal::INSTANCES);

		if (res == RESULT_BLOCK_FOUND) {
			if (!VoxelCompressedData::decompress(to_span_const(_temp_compressed_block_data), _temp_block_data)) {
				ERR_PRINT("Failed to decompress instance block");
				out_results[i] = RESULT_ERROR;
				continue;
			}
			r.data = std::make_unique<VoxelInstanceBlockData>();
			if (!deserialize_instance_block_data(*r.data, to_span_const(_temp_block_data))) {
				ERR_PRINT("Failed to deserialize instance block");
				out_results[i] = RESULT_ERROR;
				continue;
			}
		}

		out_results[i] = res;
	}

	ERR_FAIL_COND(con->end_transaction() == false);

	recycle_connection(con);
}

void VoxelStreamSQLite::save_instance_blocks(Span<VoxelStreamInstanceDataRequest> p_blocks) {
	// TODO Get block size from database
	//const int bs_po2 = VoxelConstants::DEFAULT_BLOCK_SIZE_PO2;

	// First put in cache
	for (size_t i = 0; i < p_blocks.size(); ++i) {
		VoxelStreamInstanceDataRequest &r = p_blocks[i];
		_cache.save_instance_block(r.position, r.lod, std::move(r.data));
	}

	// TODO Optimization: we should consider using a serialized cache, and measure the threshold in bytes
	if (_cache.get_indicative_block_count() >= CACHE_SIZE) {
		flush_cache();
	}
}

int VoxelStreamSQLite::get_used_channels_mask() const {
	// Assuming all, since that stream can store anything.
	return VoxelBufferInternal::ALL_CHANNELS_MASK;
}

void VoxelStreamSQLite::flush_cache() {
	VoxelStreamSQLiteInternal *con = get_connection();
	ERR_FAIL_COND(con == nullptr);
	flush_cache(con);
	recycle_connection(con);
}

// This function does not lock any mutex for internal use.
void VoxelStreamSQLite::flush_cache(VoxelStreamSQLiteInternal *con) {
	VOXEL_PROFILE_SCOPE();
	PRINT_VERBOSE(String("VoxelStreamSQLite: Flushing cache ({0} elements)")
						  .format(varray(_cache.get_indicative_block_count())));

	VoxelBlockSerializerInternal &serializer = _voxel_block_serializer;

	ERR_FAIL_COND(con == nullptr);
	ERR_FAIL_COND(con->begin_transaction() == false);

	std::vector<uint8_t> &temp_data = _temp_block_data;
	std::vector<uint8_t> &temp_compressed_data = _temp_compressed_block_data;

	// TODO Needs better error rollback handling
	_cache.flush([&serializer, con, &temp_data, &temp_compressed_data](VoxelStreamCache::Block &block) {
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
				con->save_block(loc, empty, VoxelStreamSQLiteInternal::VOXELS);
			} else {
				VoxelBlockSerializerInternal::SerializeResult res = serializer.serialize_and_compress(block.voxels);
				ERR_FAIL_COND(!res.success);
				con->save_block(loc, res.data, VoxelStreamSQLiteInternal::VOXELS);
			}
		}

		// Save instances
		temp_compressed_data.clear();
		if (block.instances != nullptr) {
			temp_data.clear();

			ERR_FAIL_COND(!serialize_instance_block_data(*block.instances, temp_data));

			ERR_FAIL_COND(!VoxelCompressedData::compress(
					to_span_const(temp_data), temp_compressed_data, VoxelCompressedData::COMPRESSION_NONE));
		}
		con->save_block(loc, temp_compressed_data, VoxelStreamSQLiteInternal::INSTANCES);

		// TODO Optimization: add a version of the query that can update both at once
	});

	ERR_FAIL_COND(con->end_transaction() == false);
}

VoxelStreamSQLiteInternal *VoxelStreamSQLite::get_connection() {
	_connection_mutex.lock();
	if (_connection_path.empty()) {
		_connection_mutex.unlock();
		return nullptr;
	}
	if (_connection_pool.size() != 0) {
		VoxelStreamSQLiteInternal *s = _connection_pool.back();
		_connection_pool.pop_back();
		_connection_mutex.unlock();
		return s;
	}
	String fpath = _connection_path;
	_connection_mutex.unlock();

	if (fpath.empty()) {
		return nullptr;
	}
	VoxelStreamSQLiteInternal *con = new VoxelStreamSQLiteInternal();
	CharString fpath_utf8 = fpath.utf8();
	if (!con->open(fpath_utf8)) {
		delete con;
		con = nullptr;
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

void VoxelStreamSQLite::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database_path", "path"), &VoxelStreamSQLite::set_database_path);
	ClassDB::bind_method(D_METHOD("get_database_path"), &VoxelStreamSQLite::get_database_path);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "database_path", PROPERTY_HINT_FILE),
			"set_database_path", "get_database_path");
}
