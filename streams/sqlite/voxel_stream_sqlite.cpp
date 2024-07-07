#include "voxel_stream_sqlite.h"
#include "../../util/godot/classes/project_settings.h"
#include "../../util/godot/core/string.h"
#include "../../util/profiling.h"
#include "../../util/string/format.h"
#include "../../util/string/std_string.h"
#include "../compressed_data.h"
#include "connection.h"

#include <string_view>
#include <unordered_set>

namespace zylann::voxel {

using namespace sqlite;

namespace {
StdVector<uint8_t> &get_tls_temp_block_data() {
	thread_local StdVector<uint8_t> tls_temp_block_data;
	return tls_temp_block_data;
}
StdVector<uint8_t> &get_tls_temp_compressed_block_data() {
	thread_local StdVector<uint8_t> tls_temp_compressed_block_data;
	return tls_temp_compressed_block_data;
}

BlockLocation::CoordinateFormat to_internal_coordinate_format(VoxelStreamSQLite::CoordinateFormat format) {
	return static_cast<BlockLocation::CoordinateFormat>(format);
}

VoxelStreamSQLite::CoordinateFormat to_exposed_coordinate_format(BlockLocation::CoordinateFormat format) {
	return static_cast<VoxelStreamSQLite::CoordinateFormat>(format);
}

bool validate_range(Vector3i pos, unsigned int lod_index, const Box3i coordinate_range, unsigned int lod_count) {
	if (!coordinate_range.contains(pos)) {
		ZN_PRINT_ERROR(format("Block position {} is outside of supported range {}", pos, coordinate_range));
		return false;
	}
	if (lod_index >= lod_count) {
		ZN_PRINT_ERROR(format("Block LOD {} is outside of supported range [0..{})", lod_index, lod_count));
		return false;
	}
	return true;
}

} // namespace

VoxelStreamSQLite::VoxelStreamSQLite() {}

VoxelStreamSQLite::~VoxelStreamSQLite() {
	ZN_PRINT_VERBOSE("~VoxelStreamSQLite");
	if (!_globalized_connection_path.empty() && _cache.get_indicative_block_count() > 0) {
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
	if (path == _user_specified_connection_path) {
		return;
	}
	if (!_globalized_connection_path.empty() && _cache.get_indicative_block_count() > 0) {
		// Save cached data before changing the path.
		// Not using get_connection() because it locks, we are already locked.
		sqlite::Connection con;
		// Note, the path could be invalid,
		// Since Godot helpfully sets the property for every character typed in the inspector.
		// So there can be lots of errors in the editor if you type it.
		if (con.open(_globalized_connection_path.data(), to_internal_coordinate_format(_preferred_coordinate_format))) {
			flush_cache_to_connection(&con);
		}
	}
	for (auto it = _connection_pool.begin(); it != _connection_pool.end(); ++it) {
		delete *it;
	}
	_block_keys_cache.clear();
	_connection_pool.clear();

	_user_specified_connection_path = path;
	// To support Godot shortcuts like `user://` and `res://` (though the latter won't work on exported builds)
	_globalized_connection_path = zylann::godot::to_std_string(ProjectSettings::get_singleton()->globalize_path(path));

	// Don't actually open anything here. We'll do it only when necessary
}

String VoxelStreamSQLite::get_database_path() const {
	MutexLock lock(_connection_mutex);
	return _user_specified_connection_path;
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
	sqlite::Connection *con = get_connection();
	ERR_FAIL_COND(con == nullptr);

	// Check the cache first
	StdVector<unsigned int> blocks_to_load;
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		VoxelStream::VoxelQueryData &q = p_blocks[i];
		const Vector3i pos = q.position_in_blocks;

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

		const ResultCode res = con->load_block(loc, temp_block_data, sqlite::Connection::VOXELS);

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
	sqlite::Connection *con = get_connection();
	const BlockLocation::CoordinateFormat coordinate_format = con->get_meta().coordinate_format;
	const Box3i coordinate_range = BlockLocation::get_coordinate_range(coordinate_format);
	const unsigned int lod_count = BlockLocation::get_lod_count(coordinate_format);
	recycle_connection(con);

	// First put in cache
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		VoxelStream::VoxelQueryData &q = p_blocks[i];
		const Vector3i pos = q.position_in_blocks;

		if (!validate_range(pos, q.lod_index, coordinate_range, lod_count)) {
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

	sqlite::Connection *con = get_connection();
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

		const ResultCode res = con->load_block(loc, temp_compressed_block_data, sqlite::Connection::INSTANCES);

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
	sqlite::Connection *con = get_connection();
	const BlockLocation::CoordinateFormat coordinate_format = con->get_meta().coordinate_format;
	const Box3i coordinate_range = BlockLocation::get_coordinate_range(coordinate_format);
	const unsigned int lod_count = BlockLocation::get_lod_count(coordinate_format);
	recycle_connection(con);

	// First put in cache
	for (size_t i = 0; i < p_blocks.size(); ++i) {
		VoxelStream::InstancesQueryData &q = p_blocks[i];

		if (!validate_range(q.position_in_blocks, q.lod_index, coordinate_range, lod_count)) {
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

	sqlite::Connection *con = get_connection();
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
	sqlite::Connection *con = get_connection();
	ERR_FAIL_COND(con == nullptr);
	flush_cache_to_connection(con);
	recycle_connection(con);
}

void VoxelStreamSQLite::flush() {
	flush_cache();
}

// This function does not lock any mutex for internal use.
void VoxelStreamSQLite::flush_cache_to_connection(sqlite::Connection *p_connection) {
	ZN_PROFILE_SCOPE();
	ZN_PRINT_VERBOSE(format("VoxelStreamSQLite: Flushing cache ({} elements)", _cache.get_indicative_block_count()));

	ERR_FAIL_COND(p_connection == nullptr);
	ERR_FAIL_COND(p_connection->begin_transaction() == false);

	StdVector<uint8_t> &temp_data = get_tls_temp_block_data();
	StdVector<uint8_t> &temp_compressed_data = get_tls_temp_compressed_block_data();

	const BlockLocation::CoordinateFormat coordinate_format = p_connection->get_meta().coordinate_format;
	const Box3i coordinate_range = BlockLocation::get_coordinate_range(coordinate_format);
	const unsigned int lod_count = BlockLocation::get_lod_count(coordinate_format);

	// TODO Needs better error rollback handling
	_cache.flush([p_connection, &temp_data, &temp_compressed_data, coordinate_range, lod_count](
						 VoxelStreamCache::Block &block
				 ) {
		ZN_ASSERT_RETURN(validate_range(block.position, block.lod, coordinate_range, lod_count));

		BlockLocation loc;
		loc.position = block.position;
		loc.lod = block.lod;

		// Save voxels
		if (block.has_voxels) {
			if (block.voxels_deleted) {
				p_connection->save_block(loc, Span<const uint8_t>(), sqlite::Connection::VOXELS);
			} else {
				BlockSerializer::SerializeResult res = BlockSerializer::serialize_and_compress(block.voxels);
				ERR_FAIL_COND(!res.success);
				p_connection->save_block(loc, to_span(res.data), sqlite::Connection::VOXELS);
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
		p_connection->save_block(loc, to_span(temp_compressed_data), sqlite::Connection::INSTANCES);

		// TODO Optimization: add a version of the query that can update both at once
	});

	ERR_FAIL_COND(p_connection->end_transaction() == false);
}

Connection *VoxelStreamSQLite::get_connection() {
	StdString fpath;
	CoordinateFormat preferred_coordinate_format;
	{
		MutexLock mlock(_connection_mutex);

		if (_globalized_connection_path.empty()) {
			return nullptr;
		}
		if (_connection_pool.size() != 0) {
			sqlite::Connection *existing_connection = _connection_pool.back();
			_connection_pool.pop_back();
			return existing_connection;
		}
		// First connection we get since we set the database path
		fpath = _globalized_connection_path;
		preferred_coordinate_format = _preferred_coordinate_format;
	}

	if (fpath.empty()) {
		return nullptr;
	}
	sqlite::Connection *con = new sqlite::Connection();
	if (!con->open(fpath.data(), to_internal_coordinate_format(preferred_coordinate_format))) {
		delete con;
		return nullptr;
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

void VoxelStreamSQLite::recycle_connection(sqlite::Connection *con) {
	const char *con_path = con->get_opened_file_path();
	// Put back in the pool if the connection path didn't change
	{
		MutexLock mlock(_connection_mutex);
		if (_globalized_connection_path == con_path) {
			_connection_pool.push_back(con);
			return;
		}
	}
	delete con;
}

void VoxelStreamSQLite::set_key_cache_enabled(bool enable) {
	_block_keys_cache_enabled = enable;
}

bool VoxelStreamSQLite::is_key_cache_enabled() const {
	return _block_keys_cache_enabled;
}

Box3i VoxelStreamSQLite::get_supported_block_range() const {
	// const Connection *con = get_connection();
	// const CoordinateFormat format = con != nullptr ? con->get_meta().coordinate_format :
	// _preferred_coordinate_format;
	const CoordinateFormat format = _preferred_coordinate_format;
	return BlockLocation::get_coordinate_range(to_internal_coordinate_format(format));
}

int VoxelStreamSQLite::get_lod_count() const {
	// const Connection *con = get_connection();
	// const CoordinateFormat format = con != nullptr ? con->get_meta().coordinate_format :
	// _preferred_coordinate_format;
	const CoordinateFormat format = _preferred_coordinate_format;
	return BlockLocation::get_lod_count(to_internal_coordinate_format(format));
}

void VoxelStreamSQLite::set_preferred_coordinate_format(CoordinateFormat format) {
	ZN_ASSERT_RETURN(format >= 0 && format < COORDINATE_FORMAT_COUNT);
	_preferred_coordinate_format = format;
}

VoxelStreamSQLite::CoordinateFormat VoxelStreamSQLite::get_preferred_coordinate_format() const {
	return _preferred_coordinate_format;
}

VoxelStreamSQLite::CoordinateFormat VoxelStreamSQLite::get_current_coordinate_format() {
	const sqlite::Connection *con = get_connection();
	if (con == nullptr) {
		return get_preferred_coordinate_format();
	}
	return to_exposed_coordinate_format(con->get_meta().coordinate_format);
}

bool VoxelStreamSQLite::copy_blocks_to_other_sqlite_stream(Ref<VoxelStreamSQLite> dst_stream) {
	// This function may be used as a generic way to migrate an old save to a new one, when the format of the old one
	// needs to change. If it's just a version change, it might be possible to do it in-place, however changes like
	// coordinate format affect primary keys, so not doing it in-place is easier.

	ZN_ASSERT_RETURN_V(dst_stream.is_valid(), false);
	ZN_ASSERT_RETURN_V(dst_stream.ptr() != this, false);

	sqlite::Connection *src_con = get_connection();
	ZN_ASSERT_RETURN_V(src_con != nullptr, false);

	ZN_ASSERT_RETURN_V(dst_stream->get_database_path() != get_database_path(), false);

	ZN_ASSERT_RETURN_V_MSG(
			dst_stream->get_block_size_po2() != get_block_size_po2(),
			false,
			"Copying between streams of different block sizes is not supported"
	);

	// We can skip deserialization and copy data blocks directly.
	// We also don't use cache.

	struct Context {
		sqlite::Connection *dst_con;

		static void save(
				void *cb_data,
				BlockLocation location,
				Span<const uint8_t> voxel_data,
				Span<const uint8_t> instances_data
		) {
			Context *ctx = static_cast<Context *>(cb_data);
			ctx->dst_con->save_block(location, voxel_data, sqlite::Connection::VOXELS);
			ctx->dst_con->save_block(location, instances_data, sqlite::Connection::INSTANCES);
		}
	};

	Context context;
	context.dst_con = dst_stream->get_connection();

	return src_con->load_all_blocks(&context, Context::save);
}

void VoxelStreamSQLite::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database_path", "path"), &VoxelStreamSQLite::set_database_path);
	ClassDB::bind_method(D_METHOD("get_database_path"), &VoxelStreamSQLite::get_database_path);

	ClassDB::bind_method(D_METHOD("set_key_cache_enabled", "enabled"), &VoxelStreamSQLite::set_key_cache_enabled);
	ClassDB::bind_method(D_METHOD("is_key_cache_enabled"), &VoxelStreamSQLite::is_key_cache_enabled);

	ClassDB::bind_method(
			D_METHOD("set_preferred_coordinate_format", "format"), &VoxelStreamSQLite::set_preferred_coordinate_format
	);
	ClassDB::bind_method(
			D_METHOD("get_preferred_coordinate_format"), &VoxelStreamSQLite::get_preferred_coordinate_format
	);

	BIND_ENUM_CONSTANT(COORDINATE_FORMAT_INT64_X16_Y16_Z16_L16);
	BIND_ENUM_CONSTANT(COORDINATE_FORMAT_INT64_X19_Y19_Z19_L7);
	BIND_ENUM_CONSTANT(COORDINATE_FORMAT_STRING_CSD);
	BIND_ENUM_CONSTANT(COORDINATE_FORMAT_BLOB80_X25_Y25_Z25_L5);
	BIND_ENUM_CONSTANT(COORDINATE_FORMAT_COUNT);

	ADD_PROPERTY(
			PropertyInfo(Variant::STRING, "database_path", PROPERTY_HINT_FILE), "set_database_path", "get_database_path"
	);

	ADD_PROPERTY(
			PropertyInfo(
					Variant::INT,
					"preferred_coordinate_format",
					PROPERTY_HINT_ENUM,
					"Int64_X16_Y16_Z16_LOD16,Int64_X19_Y19_Z19_LOD7,String_CSD,Blob80_X25_Y25_Z25_LOD5"
			),
			"set_database_path",
			"get_database_path"
	);
}

} // namespace zylann::voxel
