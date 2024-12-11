#include "connection.h"
#include "../../thirdparty/sqlite/sqlite3.h"
#include "../../util/profiling.h"
#include "../../util/string/format.h"

namespace zylann::voxel::sqlite {

namespace {

enum CoordinateColumnType {
	COORDINATE_COLUMN_U64,
	COORDINATE_COLUMN_STRING,
	COORDINATE_COLUMN_BLOB,
};

inline CoordinateColumnType get_coordinate_column_type(BlockLocation::CoordinateFormat cf) {
	switch (cf) {
		case BlockLocation::FORMAT_INT64_X16_Y16_Z16_L16:
		case BlockLocation::FORMAT_INT64_X19_Y19_Z19_L7:
			return COORDINATE_COLUMN_U64;
		case BlockLocation::FORMAT_STRING_CSD:
			return COORDINATE_COLUMN_STRING;
		case BlockLocation::FORMAT_BLOB80_X25_Y25_Z25_L5:
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
			const BlockLocation::CoordinateFormat coordinate_format,
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
		const BlockLocation::CoordinateFormat coordinate_format,
		const CoordinateColumnType key_column_type,
		sqlite3_stmt *statement,
		const int param_index,
		BlockLocation &out_location
) {
	switch (key_column_type) {
		case COORDINATE_COLUMN_U64: {
			const uint64_t eloc = sqlite3_column_int64(statement, param_index);
			out_location = BlockLocation::decode_u64(eloc, coordinate_format);
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
	Connection &db;
	TransactionScope(Connection &p_db) : db(p_db) {
		db.begin_transaction();
	}
	~TransactionScope() {
		db.end_transaction();
	}
};

static bool prepare(sqlite3 *db, sqlite3_stmt **s, const char *sql) {
	const int rc = sqlite3_prepare_v2(db, sql, -1, s, nullptr);
	if (rc != SQLITE_OK) {
		ZN_PRINT_ERROR(format("Preparing statement failed: {}", sqlite3_errmsg(db)));
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

} // namespace

Connection::Connection() {}

Connection::~Connection() {
	close();
}

bool Connection::open(const char *fpath, const BlockLocation::CoordinateFormat preferred_coordinate_format) {
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
			ZN_PRINT_ERROR(format("Failed to create table: {}", error_message));
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
		if (meta.version > VERSION_LATEST) {
			ZN_PRINT_ERROR(format(
					"Could not use database at path \"{}\", its version ({}) is higher than the latest supported ({})",
					fpath,
					meta.version,
					VERSION_LATEST
			));
			close();
			return false;
		}
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

void Connection::close() {
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

const char *Connection::get_file_path() const {
	if (_db == nullptr) {
		return nullptr;
	}
	return sqlite3_db_filename(_db, nullptr);
}

bool Connection::begin_transaction() {
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

bool Connection::end_transaction() {
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

bool Connection::save_block(const BlockLocation loc, const Span<const uint8_t> block_data, const BlockType type) {
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
			update_block_statement = nullptr;
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

VoxelStream::ResultCode Connection::load_block(
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
			get_block_statement = nullptr;
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

bool Connection::load_all_blocks(
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
			ZN_ASSERT_CONTINUE(
					read_block_location(_meta.coordinate_format, key_column_type, load_all_blocks_statement, 0, loc)
			);

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

bool Connection::load_all_block_keys(
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
			ZN_ASSERT_CONTINUE(
					read_block_location(_meta.coordinate_format, key_column_type, load_all_block_keys_statement, 0, loc)
			);

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

int Connection::load_version() {
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

Connection::Meta Connection::load_meta() {
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
			meta.coordinate_format = BlockLocation::FORMAT_INT64_X16_Y16_Z16_L16;
		} else if (meta.version == VERSION_LATEST) {
			meta.coordinate_format =
					static_cast<BlockLocation::CoordinateFormat>(sqlite3_column_int(load_meta_statement, 2));
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
			meta.coordinate_format >= 0 && meta.coordinate_format < BlockLocation::FORMAT_COUNT,
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
				ZN_PRINT_ERROR(format("Channel index {} is invalid", index));
				continue;
			}
			if (depth < 0 || depth >= VoxelBuffer::DEPTH_COUNT) {
				ZN_PRINT_ERROR(format("Depth {} is invalid", depth));
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

void Connection::save_meta(Meta meta) {
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

bool Connection::migrate_from_v0_to_v1() {
	if (_meta.version == VERSION_V1) {
		ZN_PRINT_WARNING("Version already matching");
		return true;
	}
	ZN_ASSERT_RETURN_V(_meta.version == VERSION_V0, false);

	// Prepare statements
	struct Statements {
		Connection &db;
		sqlite3_stmt *alter_table = nullptr;
		sqlite3_stmt *update_table = nullptr;

		Statements(Connection &p_db) : db(p_db) {}

		~Statements() {
			finalize(alter_table);
			finalize(update_table);
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

bool Connection::migrate_to_next_version() {
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

void Connection::migrate_to_latest_version() {
	ZN_ASSERT_RETURN(is_open());

	while (_meta.version != VERSION_LATEST) {
		const int prev = _meta.version;
		ZN_ASSERT_RETURN(migrate_to_next_version());
		ZN_ASSERT_RETURN(prev != _meta.version);
	}
}

} // namespace zylann::voxel::sqlite
