#ifndef VOXEL_STREAM_SQLITE_H
#define VOXEL_STREAM_SQLITE_H

#include "../../util/containers/std_unordered_set.h"
#include "../../util/containers/std_vector.h"
#include "../../util/string/std_string.h"
#include "../../util/thread/mutex.h"
#include "../voxel_block_serializer.h"
#include "../voxel_stream.h"
#include "../voxel_stream_cache.h"

namespace zylann::voxel::sqlite {
class Connection;
}

namespace zylann::voxel {

// Saves voxel data into a single SQLite database file.
class VoxelStreamSQLite : public VoxelStream {
	GDCLASS(VoxelStreamSQLite, VoxelStream)
public:
	static const unsigned int CACHE_SIZE = 64;

	VoxelStreamSQLite();
	~VoxelStreamSQLite();

	// Warning: changing this path from a valid one to another is not always safe in a multithreaded context.
	// If threads were about to write into database A but it gets changed to database B,
	// that remaining data will get written in database B.
	// The nominal use case is to set this path when the game starts and not change it until the end of the session.
	void set_database_path(String path);
	String get_database_path() const;

	void load_voxel_block(VoxelStream::VoxelQueryData &q) override;
	void save_voxel_block(VoxelStream::VoxelQueryData &q) override;

	void load_voxel_blocks(Span<VoxelStream::VoxelQueryData> p_blocks) override;
	void save_voxel_blocks(Span<VoxelStream::VoxelQueryData> p_blocks) override;

#ifdef VOXEL_ENABLE_INSTANCER
	bool supports_instance_blocks() const override;
	void load_instance_blocks(Span<VoxelStream::InstancesQueryData> out_blocks) override;
	void save_instance_blocks(Span<VoxelStream::InstancesQueryData> p_blocks) override;
#endif

	bool supports_loading_all_blocks() const override {
		return true;
	}
	void load_all_blocks(FullLoadingResult &result) override;

	int get_used_channels_mask() const override;

	void flush() override;
	void flush_cache();

	// Might improve query performance if saved data is very sparse (like when only edited blocks are saved).
	void set_key_cache_enabled(bool enable);
	bool is_key_cache_enabled() const;

	Box3i get_supported_block_range() const override;
	int get_lod_count() const override;

	enum CoordinateFormat {
		COORDINATE_FORMAT_INT64_X16_Y16_Z16_L16 = 0,
		COORDINATE_FORMAT_INT64_X19_Y19_Z19_L7,
		COORDINATE_FORMAT_STRING_CSD,
		COORDINATE_FORMAT_BLOB80_X25_Y25_Z25_L5,
		COORDINATE_FORMAT_COUNT
	};

	void set_preferred_coordinate_format(CoordinateFormat format);
	CoordinateFormat get_preferred_coordinate_format() const;

	CoordinateFormat get_current_coordinate_format();

	bool copy_blocks_to_other_sqlite_stream(Ref<VoxelStreamSQLite> dst_stream);

private:
	void rebuild_key_cache();

	struct BlockKeysCache {
		FixedArray<StdUnorderedSet<Vector3i>, constants::MAX_LOD> lods;
		RWLock rw_lock;

		inline bool contains(Vector3i bpos, unsigned int lod_index) const {
			const StdUnorderedSet<Vector3i> &keys = lods[lod_index];
			RWLockRead rlock(rw_lock);
			return keys.find(bpos) != keys.end();
		}

		inline void add_no_lock(Vector3i bpos, unsigned int lod_index) {
			lods[lod_index].insert(bpos);
		}

		inline void add(Vector3i bpos, unsigned int lod_index) {
			RWLockWrite wlock(rw_lock);
			add_no_lock(bpos, lod_index);
		}

		inline void clear() {
			RWLockWrite wlock(rw_lock);
			for (unsigned int i = 0; i < lods.size(); ++i) {
				lods[i].clear();
			}
		}

		// inline size_t get_memory_usage() const {
		// 	size_t mem = 0;
		// 	for (unsigned int i = 0; i < lods.size(); ++i) {
		// 		const StdUnorderedSet<Vector3i> &keys = lods[i];
		// 		mem += sizeof(Vector3i) * keys.size();
		// 	}
		// 	return mem;
		// }
	};

	// An SQlite3 database is safe to use with multiple threads in serialized mode,
	// but after having a look at the implementation while stepping with a debugger, here are what actually happens:
	//
	// 1) Prepared statements might be safe to use in multiple threads, but the end result isn't safe.
	//    Thread A could bind a value, then thread B could bind another value replacing the first before thread A
	//    executes the statement. So in the end, each thread should get its own set of statements.
	//
	// 2) Executing a statement locks the entire database with a mutex.
	//    So indeed access is serialized, in the sense that CPU work will execute in series, not in parallel.
	//    in other words, you loose the speed of multi-threading.
	//
	// Because of this, in our use case, it might be simpler to just leave SQLite in thread-safe mode,
	// and synchronize ourselves.

	struct ConnectionResult {
		enum Code { SUCCESS, NOT_CONFIGURED, ERROR };

		sqlite::Connection *connection = nullptr;
		Code code = ERROR;
	};

	ConnectionResult get_connection();
	void recycle_connection(sqlite::Connection *con);

	struct ScopeRecycle {
		VoxelStreamSQLite *stream;
		sqlite::Connection *connection;

		ScopeRecycle(VoxelStreamSQLite *p_stream, sqlite::Connection *p_connection) :
				stream(p_stream), connection(p_connection) {
#ifdef DEV_ENABLED
			ZN_ASSERT(stream != nullptr);
			ZN_ASSERT(connection != nullptr);
#endif
		}

		~ScopeRecycle() {
			stream->recycle_connection(connection);
		}
	};

	void flush_cache_to_connection(sqlite::Connection *p_connection);

	static void _bind_methods();

	String _user_specified_connection_path;
	StdString _globalized_connection_path;
	StdVector<sqlite::Connection *> _connection_pool;
	Mutex _connection_mutex;
	// This cache stores blocks in memory, and gets flushed to the database when big enough.
	// This is because save queries are more expensive.
	// It also speeds up queries of blocks that were recently saved.
	VoxelStreamCache _cache;
	// The current way we stream data is by querying every block location near each player, to know if there is data.
	// Therefore testing if a block is present is the beginning of the most frequently executed code path.
	// In configurations where only edited blocks get saved, very few blocks even get stored in the database,
	// so it makes sense to cache keys to make this query fast and concurrent.
	// Note: in the long term, on a game that systematically saves everything it generates instead of just edits,
	// such a cache can become quite large. In this case we could either allow turning it off, or use an octree.
	BlockKeysCache _block_keys_cache;
	bool _block_keys_cache_enabled = false;
	// Format that will be used when creating new databases. May not necessarily match the format actually used by
	// existing databases.
	CoordinateFormat _preferred_coordinate_format = COORDINATE_FORMAT_STRING_CSD;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelStreamSQLite::CoordinateFormat);

#endif // VOXEL_STREAM_SQLITE_H
