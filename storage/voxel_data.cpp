#include "voxel_data.h"
#include "../util/dstack.h"
#include "../util/math/conv.h"
#include "../util/string_funcs.h"
#include "../util/thread/mutex.h"
#include "voxel_data_grid.h"
#include "voxel_metadata_variant.h"

namespace zylann::voxel {

namespace {
struct BeforeUnloadSaveAction {
	std::vector<VoxelData::BlockToSave> *to_save;
	Vector3i position;
	unsigned int lod_index;

	inline void operator()(VoxelDataBlock &block) {
		if (block.is_modified()) {
			// If a modified block has no voxels, it is equivalent to removing the block from the stream
			VoxelData::BlockToSave b;
			b.position = position;
			b.lod_index = lod_index;
			if (block.has_voxels()) {
				// No copy is necessary because the block will be removed anyways
				b.voxels = block.get_voxels_shared();
			}
			to_save->push_back(b);
		}
	}
};

struct ScheduleSaveAction {
	std::vector<VoxelData::BlockToSave> &blocks_to_save;
	uint8_t lod_index;
	bool with_copy;

	void operator()(const Vector3i &bpos, VoxelDataBlock &block) {
		if (block.is_modified()) {
			// print_line(String("Scheduling save for block {0}").format(varray(block->position.to_vec3())));
			VoxelData::BlockToSave b;
			// If a modified block has no voxels, it is equivalent to removing the block from the stream
			if (block.has_voxels()) {
				if (with_copy) {
					RWLockRead lock(block.get_voxels().get_lock());
					b.voxels = make_shared_instance<VoxelBufferInternal>();
					block.get_voxels_const().duplicate_to(*b.voxels, true);
				} else {
					b.voxels = block.get_voxels_shared();
				}
			}
			b.position = bpos;
			b.lod_index = lod_index;
			blocks_to_save.push_back(b);
			block.set_modified(false);
		}
	}
};
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelData::VoxelData() {}
VoxelData::~VoxelData() {}

void VoxelData::set_lod_count(unsigned int p_lod_count) {
	ZN_ASSERT(p_lod_count < constants::MAX_LOD);
	ZN_ASSERT(p_lod_count >= 1);

	// This lock can be held for longer due to resetting the maps, but it is very rare.
	// In games it is only used once on startup.
	// In editor it is more frequent but still rare enough and shouldn't be too bad if hiccups occur.
	MutexLock wlock(_settings_mutex);

	if (p_lod_count == _lod_count) {
		return;
	}

	_lod_count = p_lod_count;

	// Not entirely required, but changing LOD count at runtime is rarely needed
	reset_maps_no_settings_lock();
}

void VoxelData::reset_maps() {
	MutexLock wlock(_settings_mutex);
	reset_maps_no_settings_lock();
}

void VoxelData::reset_maps_no_settings_lock() {
	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &data_lod = _lods[lod_index];
		RWLockWrite wlock(data_lod.map_lock);
		// Instance new maps if we have more lods, or clear them otherwise
		if (lod_index < _lod_count) {
			data_lod.map.create(lod_index);
		} else {
			data_lod.map.clear();
		}
	}
}

void VoxelData::set_bounds(Box3i bounds) {
	MutexLock wlock(_settings_mutex);
	_bounds_in_voxels = bounds;
}

void VoxelData::set_generator(Ref<VoxelGenerator> generator) {
	MutexLock wlock(_settings_mutex);
	_generator = generator;
}

void VoxelData::set_stream(Ref<VoxelStream> stream) {
	MutexLock wlock(_settings_mutex);
	_stream = stream;
}

void VoxelData::set_streaming_enabled(bool enabled) {
	_streaming_enabled = enabled;
}

inline VoxelSingleValue get_voxel_with_lock(VoxelBufferInternal &vb, Vector3i pos, unsigned int channel) {
	VoxelSingleValue v;
	if (channel == VoxelBufferInternal::CHANNEL_SDF) {
		RWLockRead rlock(vb.get_lock());
		v.f = vb.get_voxel_f(pos.x, pos.y, pos.z, channel);
	} else {
		RWLockRead rlock(vb.get_lock());
		v.i = vb.get_voxel(pos, channel);
	}
	return v;
}

// TODO Piggyback on `copy`? The implementation is quite complex, and it's not supposed to be an efficient use case
VoxelSingleValue VoxelData::get_voxel(Vector3i pos, unsigned int channel_index, VoxelSingleValue defval) const {
	ZN_PROFILE_SCOPE();

	if (!_bounds_in_voxels.contains(pos)) {
		return defval;
	}

	Vector3i block_pos = pos >> get_block_size_po2();
	bool generate = false;

	if (!_streaming_enabled) {
		const Lod &data_lod0 = _lods[0];
		std::shared_ptr<VoxelBufferInternal> voxels = try_get_voxel_buffer_with_lock(data_lod0, block_pos, generate);

		if (voxels == nullptr) {
			// No voxel data. We know everything is loaded when data streaming is not used, so try to generate directly.
			// TODO We should be able to get a value if modifiers are used but not a base generator
			Ref<VoxelGenerator> generator = get_generator();
			if (generator.is_valid()) {
				VoxelSingleValue value = generator->generate_single(pos, channel_index);
				if (channel_index == VoxelBufferInternal::CHANNEL_SDF) {
					float sdf = value.f;
					_modifiers.apply(sdf, to_vec3(pos));
					value.f = sdf;
				}
				return value;
			}
		} else {
			const Vector3i rpos = data_lod0.map.to_local(pos);
			return get_voxel_with_lock(*voxels, rpos, channel_index);
		}
		return defval;

	} else {
		// When data streaming is used, we try to find voxel data. If we don't and the location is also not loaded, we
		// have to return the default value.
		Vector3i voxel_pos = pos;
		Ref<VoxelGenerator> generator = get_generator();
		const unsigned int lod_count = get_lod_count();

		// Check all LODs until we find a loaded location
		for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
			const Lod &data_lod = _lods[lod_index];

			std::shared_ptr<VoxelBufferInternal> voxels = try_get_voxel_buffer_with_lock(data_lod, block_pos, generate);

			if (voxels != nullptr) {
				return get_voxel_with_lock(*voxels, data_lod.map.to_local(voxel_pos), channel_index);

			} else if (generate) {
				// TODO We should be able to get a value if modifiers are used but not a base generator
				if (generator.is_valid()) {
					VoxelSingleValue value = generator->generate_single(pos, channel_index);
					if (channel_index == VoxelBufferInternal::CHANNEL_SDF) {
						float sdf = value.f;
						_modifiers.apply(sdf, to_vec3(pos));
						value.f = sdf;
					}
					return value;
				} else {
					return defval;
				}
			}

			// Fallback on lower LOD
			block_pos = block_pos >> 1;
			voxel_pos = voxel_pos >> 1;
		}
		return defval;
	}
}

// TODO Piggyback on `paste`? The implementation is quite complex, and it's not supposed to be an efficient use case
bool VoxelData::try_set_voxel(uint64_t value, Vector3i pos, unsigned int channel_index) {
	const Vector3i block_pos_lod0 = pos >> get_block_size_po2();
	Lod &data_lod0 = _lods[0];
	const Vector3i block_pos = data_lod0.map.voxel_to_block(pos);

	bool can_generate = false;
	std::shared_ptr<VoxelBufferInternal> voxels = try_get_voxel_buffer_with_lock(data_lod0, block_pos, can_generate);

	if (voxels == nullptr) {
		if (_streaming_enabled && !can_generate) {
			return false;
		}
		Ref<VoxelGenerator> generator = get_generator();
		if (generator.is_valid()) {
			voxels = make_shared_instance<VoxelBufferInternal>();
			voxels->create(Vector3iUtil::create(get_block_size()));
			VoxelGenerator::VoxelQueryData q{ *voxels, pos, 0 };
			generator->generate_block(q);

			_modifiers.apply(q.voxel_buffer, AABB(pos, q.voxel_buffer.get_size()));

			RWLockWrite wlock(data_lod0.map_lock);
			if (data_lod0.map.has_block(block_pos_lod0)) {
				// A block was loaded by another thread, cancel our edit.
				return false;
			}

			data_lod0.map.set_block_buffer(block_pos_lod0, voxels, true);
		}
	}
	// If it turns out to be a problem, use CoW?
	RWLockWrite lock(voxels->get_lock());
	voxels->set_voxel(value, data_lod0.map.to_local(pos), channel_index);
	// We don't update mips, this must be done by the caller
	return true;
}

float VoxelData::get_voxel_f(Vector3i pos, unsigned int channel_index) const {
	VoxelSingleValue defval;
	defval.f = 1.f;
	return get_voxel(pos, channel_index, defval).f;
}

bool VoxelData::try_set_voxel_f(real_t value, Vector3i pos, unsigned int channel_index) {
	// TODO Handle format instead of hardcoding 16-bits
	return try_set_voxel(snorm_to_s16(value), pos, channel_index);
}

void VoxelData::copy(Vector3i min_pos, VoxelBufferInternal &dst_buffer, unsigned int channels_mask) const {
	ZN_PROFILE_SCOPE();
	const Lod &data_lod0 = _lods[0];
	const VoxelModifierStack &modifiers = _modifiers;

	Ref<VoxelGenerator> generator = get_generator();

	if (is_streaming_enabled() || generator.is_null()) {
		RWLockRead rlock(data_lod0.map_lock);
		// Only gets blocks we have voxel data of. Other blocks will be air.
		// TODO Maybe in the end we should just do the same in either case?
		data_lod0.map.copy(min_pos, dst_buffer, channels_mask);

	} else {
		struct GenContext {
			VoxelGenerator &generator;
			const VoxelModifierStack &modifiers;
		};

		GenContext gctx{ **generator, modifiers };

		RWLockRead rlock(data_lod0.map_lock);
		data_lod0.map.copy(min_pos, dst_buffer, channels_mask, &gctx,
				// Generate on the fly in areas where blocks aren't edited
				[](void *callback_data, VoxelBufferInternal &voxels, Vector3i pos) {
					// Suffixed with `2` because GCC warns it shadows a previous local...
					GenContext *gctx2 = reinterpret_cast<GenContext *>(callback_data);
					VoxelGenerator::VoxelQueryData q{ voxels, pos, 0 };
					gctx2->generator.generate_block(q);
					gctx2->modifiers.apply(voxels, AABB(pos, voxels.get_size()));
				});
	}
}

void VoxelData::paste(
		Vector3i min_pos, const VoxelBufferInternal &src_buffer, unsigned int channels_mask, bool create_new_blocks) {
	ZN_PROFILE_SCOPE();
	Lod &data_lod0 = _lods[0];
	data_lod0.map.paste(min_pos, src_buffer, channels_mask, false, 0, 0, create_new_blocks);
}

void VoxelData::paste_masked(Vector3i min_pos, const VoxelBufferInternal &src_buffer, unsigned int channels_mask,
		uint8_t mask_channel, uint64_t mask_value, bool create_new_blocks) {
	ZN_PROFILE_SCOPE();
	Lod &data_lod0 = _lods[0];
	data_lod0.map.paste(min_pos, src_buffer, channels_mask, true, mask_channel, mask_value, create_new_blocks);
}

bool VoxelData::is_area_loaded(const Box3i p_voxels_box) const {
	if (is_streaming_enabled() == false) {
		// TODO Actually, there is still a check to make because loading still takes time
		return true;
	}
	const Box3i voxel_box = p_voxels_box.clipped(get_bounds());
	const Lod &data_lod0 = _lods[0];
	{
		RWLockRead rlock(data_lod0.map_lock);
		const bool all_blocks_present = data_lod0.map.is_area_fully_loaded(voxel_box);
		// In a multi-LOD context, it is assumed the parent LOD follows the rule of covering all its children.
		// In other words, all parent LODs are assumed to be loaded.
		return all_blocks_present;
	}
}

void VoxelData::pre_generate_box(Box3i voxel_box, Span<Lod> lods, unsigned int data_block_size, bool streaming,
		unsigned int lod_count, Ref<VoxelGenerator> generator, VoxelModifierStack &modifiers) {
	ZN_PROFILE_SCOPE();
	// ERR_FAIL_COND_MSG(_full_load_mode == false, nullptr, "This function can only be used in full load mode");

	struct Task {
		Vector3i block_pos;
		uint32_t lod_index;
		std::shared_ptr<VoxelBufferInternal> voxels;
	};

	// TODO Optimize: thread_local pooling?
	std::vector<Task> todo;
	// We'll pack tasks per LOD so we'll have less locking to do
	// TODO Optimize: thread_local pooling?
	std::vector<unsigned int> count_per_lod;

	// Find empty slots
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		const Box3i block_box = voxel_box.downscaled(data_block_size << lod_index);

		// ZN_PRINT_VERBOSE(format("Preloading box {} at lod {} synchronously", block_box, lod_index));

		Lod &data_lod = lods[lod_index];
		const unsigned int prev_size = todo.size();

		{
			RWLockRead rlock(data_lod.map_lock);
			block_box.for_each_cell([&data_lod, lod_index, &todo, streaming](Vector3i block_pos) {
				// We don't check "loading blocks", because this function wants to complete the task right now.
				const VoxelDataBlock *block = data_lod.map.get_block(block_pos);
				if (streaming) {
					// Non-loaded blocks must not be touched because we don't know what's in them.
					// We can generate caches if loaded ones have no voxel data.
					if (block != nullptr && !block->has_voxels()) {
						todo.push_back(Task{ block_pos, lod_index, nullptr });
					}
				} else {
					// We can generate anywhere voxel data is not in memory
					if (block == nullptr || !block->has_voxels()) {
						todo.push_back(Task{ block_pos, lod_index, nullptr });
					}
				}
			});
		}

		count_per_lod.push_back(todo.size() - prev_size);
	}

	const Vector3i block_size = Vector3iUtil::create(data_block_size);

	// Generate
	for (unsigned int i = 0; i < todo.size(); ++i) {
		Task &task = todo[i];
		task.voxels = make_shared_instance<VoxelBufferInternal>();
		task.voxels->create(block_size);
		// TODO Format?
		if (generator.is_valid()) {
			ZN_PROFILE_SCOPE_NAMED("Generate");
			VoxelGenerator::VoxelQueryData q{ //
				*task.voxels, task.block_pos * (data_block_size << task.lod_index), task.lod_index
			};
			generator->generate_block(q);
			modifiers.apply(q.voxel_buffer, AABB(q.origin_in_voxels, q.voxel_buffer.get_size() << q.lod));
		}
	}

	// Populate slots
	unsigned int task_index = 0;
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		ZN_ASSERT(lod_index < count_per_lod.size());
		const unsigned int count = count_per_lod[lod_index];

		if (count > 0) {
			const unsigned int end_task_index = task_index + count;

			Lod &data_lod = lods[lod_index];
			RWLockWrite wlock(data_lod.map_lock);

			for (; task_index < end_task_index; ++task_index) {
				Task &task = todo[task_index];
				ZN_ASSERT(task.lod_index == lod_index);
				const VoxelDataBlock *prev_block = data_lod.map.get_block(task.block_pos);
				if (prev_block != nullptr && prev_block->has_voxels()) {
					// Sorry, that block has been set in the meantime by another thread.
					// We'll assume the block we just generated is redundant and discard it.
					continue;
				}
				data_lod.map.set_block_buffer(task.block_pos, task.voxels, true);
			}
		}
	}
}

void VoxelData::pre_generate_box(Box3i voxel_box) {
	const unsigned int data_block_size = get_block_size();
	const bool streaming = is_streaming_enabled();
	const unsigned int lod_count = get_lod_count();
	pre_generate_box(voxel_box, to_span(_lods), data_block_size, streaming, lod_count, get_generator(), _modifiers);
}

void VoxelData::clear_cached_blocks_in_voxel_area(Box3i p_voxel_box) {
	const unsigned int lod_count = get_lod_count();

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		Lod &lod = _lods[lod_index];
		RWLockRead rlock(lod.map_lock);

		const Box3i blocks_box = p_voxel_box.downscaled(lod.map.get_block_size() << lod_index);
		blocks_box.for_each_cell_zxy([&lod](const Vector3i bpos) {
			VoxelDataBlock *block = lod.map.get_block(bpos);
			if (block == nullptr || block->is_edited() || block->is_modified()) {
				return;
			}
			block->clear_voxels();
		});
	}
}

void VoxelData::mark_area_modified(Box3i p_voxel_box, std::vector<Vector3i> *lod0_new_blocks_to_lod) {
	const Box3i bbox = p_voxel_box.downscaled(get_block_size());

	Lod &data_lod0 = _lods[0];
	{
		RWLockRead rlock(data_lod0.map_lock);

		bbox.for_each_cell([this, &data_lod0, lod0_new_blocks_to_lod](Vector3i block_pos_lod0) {
			VoxelDataBlock *block = data_lod0.map.get_block(block_pos_lod0);
			// We can get null blocks due to the added padding...
			// ERR_FAIL_COND(block == nullptr);
			if (block == nullptr) {
				return;
			}
			// We can get blocks without voxels in them due to the added padding...
			if (!block->has_voxels()) {
				return;
			}

			// RWLockWrite wlock(block->get_voxels_shared()->get_lock());
			block->set_modified(true);
			block->set_edited(true);

			// TODO That boolean is also modified by the threaded update task (always set to false)
			if (!block->get_needs_lodding()) {
				block->set_needs_lodding(true);

				// This is what indirectly causes remeshing
				if (lod0_new_blocks_to_lod != nullptr) {
					lod0_new_blocks_to_lod->push_back(block_pos_lod0);
				}
			}
		});
	}
}

bool VoxelData::try_set_block(Vector3i block_position, const VoxelDataBlock &block) {
	bool inserted = true;
	try_set_block(block_position, block,
			[&inserted](VoxelDataBlock &existing, const VoxelDataBlock &incoming) { inserted = false; });
	return inserted;
}

bool VoxelData::has_block(Vector3i bpos, unsigned int lod_index) const {
	const Lod &data_lod = _lods[lod_index];
	RWLockRead rlock(data_lod.map_lock);
	return data_lod.map.has_block(bpos);
}

bool VoxelData::has_all_blocks_in_area(Box3i data_blocks_box) const {
	const Box3i bounds_in_blocks = get_bounds().downscaled(get_block_size());
	data_blocks_box = data_blocks_box.clipped(bounds_in_blocks);

	const Lod &data_lod = _lods[0];
	RWLockRead rlock(data_lod.map_lock);

	return data_blocks_box.all_cells_match([&data_lod](Vector3i bpos) { //
		return data_lod.map.has_block(bpos);
	});
}

unsigned int VoxelData::get_block_count() const {
	unsigned int sum = 0;
	const unsigned int lod_count = get_lod_count();
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		const Lod &lod = _lods[lod_index];
		RWLockRead rlock(lod.map_lock);
		sum += lod.map.get_block_count();
	}
	return sum;
}

void VoxelData::update_lods(Span<const Vector3i> modified_lod0_blocks, std::vector<BlockLocation> *out_updated_blocks) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();
	// Propagates edits performed so far to other LODs.
	// These LODs must be currently in memory, otherwise terrain data will miss it.
	// This is currently ensured by the fact we load blocks in a "pyramidal" way,
	// i.e there is no way for a block to be loaded if its parent LOD isn't loaded already.
	// In the future we may implement storing of edits to be applied later if blocks can't be found.

	const int data_block_size = get_block_size();
	const int data_block_size_po2 = get_block_size_po2();
	const unsigned int lod_count = get_lod_count();
	const bool streaming_enabled = is_streaming_enabled();
	Ref<VoxelGenerator> generator = get_generator();

	static thread_local FixedArray<std::vector<Vector3i>, constants::MAX_LOD> tls_blocks_to_process_per_lod;

	// Make sure LOD0 gets updates even if _lod_count is 1
	{
		std::vector<Vector3i> &dst_lod0 = tls_blocks_to_process_per_lod[0];
		dst_lod0.resize(modified_lod0_blocks.size());
		memcpy(dst_lod0.data(), modified_lod0_blocks.data(), dst_lod0.size() * sizeof(Vector3i));
	}
	{
		Lod &data_lod0 = _lods[0];
		RWLockRead rlock(data_lod0.map_lock);

		std::vector<Vector3i> &blocks_pending_lodding_lod0 = tls_blocks_to_process_per_lod[0];

		for (unsigned int i = 0; i < blocks_pending_lodding_lod0.size(); ++i) {
			const Vector3i data_block_pos = blocks_pending_lodding_lod0[i];
			VoxelDataBlock *data_block = data_lod0.map.get_block(data_block_pos);
			ERR_CONTINUE(data_block == nullptr);
			data_block->set_needs_lodding(false);

			if (out_updated_blocks != nullptr) {
				out_updated_blocks->push_back(BlockLocation{ data_block_pos, 0 });
			}
		}
	}

	const int half_bs = data_block_size >> 1;

	// Process downscales upwards in pairs of consecutive LODs.
	// This ensures we don't process multiple times the same blocks.
	// Only LOD0 is editable at the moment, so we'll downscale from there
	for (uint8_t dst_lod_index = 1; dst_lod_index < lod_count; ++dst_lod_index) {
		const uint8_t src_lod_index = dst_lod_index - 1;
		std::vector<Vector3i> &src_lod_blocks_to_process = tls_blocks_to_process_per_lod[src_lod_index];
		std::vector<Vector3i> &dst_lod_blocks_to_process = tls_blocks_to_process_per_lod[dst_lod_index];

		// VoxelLodTerrainUpdateData::Lod &dst_lod = state.lods[dst_lod_index];

		Lod &src_data_lod = _lods[src_lod_index];
		RWLockRead src_data_lod_map_rlock(src_data_lod.map_lock);

		Lod &dst_data_lod = _lods[dst_lod_index];
		// TODO Could take long locking this, we may generate things first and assign to the map at the end.
		// Besides, in per-block streaming mode, it is not needed because blocks are supposed to be present
		RWLockRead wlock(dst_data_lod.map_lock);

		for (unsigned int i = 0; i < src_lod_blocks_to_process.size(); ++i) {
			const Vector3i src_bpos = src_lod_blocks_to_process[i];
			const Vector3i dst_bpos = src_bpos >> 1;

			VoxelDataBlock *src_block = src_data_lod.map.get_block(src_bpos);
			VoxelDataBlock *dst_block = dst_data_lod.map.get_block(dst_bpos);

			src_block->set_needs_lodding(false);

			if (dst_block == nullptr) {
				if (!streaming_enabled) {
					// TODO Doing this on the main thread can be very demanding and cause a stall.
					// We should find a way to make it asynchronous, not need mips, or not edit outside viewers area.
					std::shared_ptr<VoxelBufferInternal> voxels = make_shared_instance<VoxelBufferInternal>();
					voxels->create(Vector3iUtil::create(data_block_size));
					VoxelGenerator::VoxelQueryData q{ //
						*voxels, //
						dst_bpos << (dst_lod_index + data_block_size_po2), //
						dst_lod_index
					};
					if (generator.is_valid()) {
						ZN_PROFILE_SCOPE_NAMED("Generate");
						generator->generate_block(q);
					}
					_modifiers.apply(
							q.voxel_buffer, AABB(q.origin_in_voxels, q.voxel_buffer.get_size() << dst_lod_index));

					dst_block = dst_data_lod.map.set_block_buffer(dst_bpos, voxels, true);

				} else {
					ZN_PRINT_ERROR(format(
							"Destination block {} not found when cascading edits on LOD {}", dst_bpos, dst_lod_index));
					continue;
				}
			}

			// The block and its lower LOD indices are expected to be available.
			// Otherwise it means the function was called too late?
			ZN_ASSERT(src_block != nullptr);
			// ZN_ASSERT(dst_block != nullptr);
			// The block should have voxels if it has been edited or mipped.
			ZN_ASSERT(src_block->has_voxels());

			if (out_updated_blocks != nullptr) {
				out_updated_blocks->push_back(BlockLocation{ dst_bpos, dst_lod_index });
			}

			dst_block->set_modified(true);

			if (dst_lod_index != lod_count - 1 && !dst_block->get_needs_lodding()) {
				dst_block->set_needs_lodding(true);
				dst_lod_blocks_to_process.push_back(dst_bpos);
			}

			const Vector3i rel = src_bpos - (dst_bpos << 1);

			// Update lower LOD
			// This must always be done after an edit before it gets saved, otherwise LODs won't match and it will look
			// ugly.
			// TODO Optimization: try to narrow to edited region instead of taking whole block
			{
				ZN_PROFILE_SCOPE_NAMED("Downscale");
				RWLockRead rlock(src_block->get_voxels().get_lock());
				src_block->get_voxels().downscale_to(
						dst_block->get_voxels(), Vector3i(), src_block->get_voxels_const().get_size(), rel * half_bs);
			}
		}

		src_lod_blocks_to_process.clear();
		// No need to clear the last list because we never add blocks to it
	}

	//	uint64_t time_spent = profiling_clock.restart();
	//	if (time_spent > 10) {
	//		print_line(String("Took {0} us to update lods").format(varray(time_spent)));
	//	}
}

void VoxelData::unload_blocks(Box3i bbox, unsigned int lod_index, std::vector<BlockToSave> *to_save) {
	Lod &lod = _lods[lod_index];
	RWLockWrite wlock(lod.map_lock);
	if (to_save == nullptr) {
		bbox.for_each_cell_zxy([&lod](Vector3i bpos) { //
			lod.map.remove_block(bpos, VoxelDataMap::NoAction());
		});
	} else {
		bbox.for_each_cell_zxy([&lod, lod_index, to_save](Vector3i bpos) {
			lod.map.remove_block(bpos, BeforeUnloadSaveAction{ to_save, bpos, lod_index });
		});
	}
}

void VoxelData::unload_blocks(Span<const Vector3i> positions, std::vector<BlockToSave> *to_save) {
	Lod &lod = _lods[0];
	RWLockWrite wlock(lod.map_lock);
	if (to_save == nullptr) {
		for (Vector3i bpos : positions) {
			lod.map.remove_block(bpos, VoxelDataMap::NoAction());
		}
	} else {
		for (Vector3i bpos : positions) {
			lod.map.remove_block(bpos, BeforeUnloadSaveAction{ to_save, bpos, 0 });
		}
	}
}

bool VoxelData::consume_block_modifications(Vector3i bpos, VoxelData::BlockToSave &out_to_save) {
	Lod &lod = _lods[0];
	RWLockRead rlock(lod.map_lock);
	VoxelDataBlock *block = lod.map.get_block(bpos);
	if (block == nullptr) {
		return false;
	}
	if (block->is_modified()) {
		if (block->has_voxels()) {
			RWLockRead lock(block->get_voxels().get_lock());
			out_to_save.voxels = make_shared_instance<VoxelBufferInternal>();
			block->get_voxels_const().duplicate_to(*out_to_save.voxels, true);
		}
		out_to_save.position = bpos;
		out_to_save.lod_index = 0;
		block->set_modified(false);
		return true;
	}
	return false;
}

void VoxelData::consume_all_modifications(std::vector<BlockToSave> &to_save, bool with_copy) {
	const unsigned int lod_count = get_lod_count();
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		Lod &lod = _lods[lod_index];
		RWLockRead rlock(lod.map_lock);
		lod.map.for_each_block(ScheduleSaveAction{ to_save, uint8_t(lod_index), with_copy });
	}
}

void VoxelData::get_missing_blocks(
		Span<const Vector3i> block_positions, unsigned int lod_index, std::vector<Vector3i> &out_missing) const {
	const Lod &lod = _lods[lod_index];
	RWLockRead rlock(lod.map_lock);
	for (const Vector3i &pos : block_positions) {
		if (!lod.map.has_block(pos)) {
			out_missing.push_back(pos);
		}
	}
}

void VoxelData::get_missing_blocks(
		Box3i p_blocks_box, unsigned int lod_index, std::vector<Vector3i> &out_missing) const {
	const Lod &data_lod = _lods[lod_index];

	const Box3i bounds_in_blocks = get_bounds().downscaled(get_block_size());
	const Box3i blocks_box = p_blocks_box.clipped(bounds_in_blocks);

	RWLockRead rlock(data_lod.map_lock);

	blocks_box.for_each_cell_zxy([&data_lod, &out_missing](Vector3i bpos) {
		if (!data_lod.map.has_block(bpos)) {
			out_missing.push_back(bpos);
		}
	});
}

void VoxelData::get_blocks_with_voxel_data(
		Box3i p_blocks_box, unsigned int lod_index, Span<std::shared_ptr<VoxelBufferInternal>> out_blocks) const {
	ZN_ASSERT(int64_t(out_blocks.size()) >= Vector3iUtil::get_volume(p_blocks_box.size));

	const Lod &data_lod = _lods[lod_index];
	RWLockRead rlock(data_lod.map_lock);

	unsigned int index = 0;

	// Iteration order matters for thread access.
	p_blocks_box.for_each_cell_zxy([&index, &data_lod, &out_blocks](Vector3i data_block_pos) {
		const VoxelDataBlock *nblock = data_lod.map.get_block(data_block_pos);
		// The block can actually be null on some occasions. Not sure yet if it's that bad
		// CRASH_COND(nblock == nullptr);
		if (nblock != nullptr && nblock->has_voxels()) {
			out_blocks[index] = nblock->get_voxels_shared();
		}
		++index;
	});
}

void VoxelData::get_blocks_grid(VoxelDataGrid &grid, Box3i box_in_voxels, unsigned int lod_index) const {
	ZN_PROFILE_SCOPE();
	const Lod &data_lod = _lods[lod_index];
	RWLockRead rlock(data_lod.map_lock);
	grid.reference_area(data_lod.map, lod_index == 0 ? box_in_voxels : box_in_voxels.downscaled(1 << lod_index));
}

bool VoxelData::has_blocks_with_voxels_in_area_broad_mip_test(Box3i box_in_voxels) const {
	ZN_PROFILE_SCOPE();

	// Find the highest LOD level to query first
	const Vector3i box_size_in_blocks = box_in_voxels.size >> get_block_size_po2();
	const int box_size_in_blocks_longest_axis =
			math::max(box_size_in_blocks.x, math::max(box_size_in_blocks.y, box_size_in_blocks.z));
	const int top_lod_index =
			math::min(math::get_next_power_of_two_32_shift(box_size_in_blocks_longest_axis), get_lod_count());

	// Find if edited mips exist
	const Lod &mip_data_lod = _lods[top_lod_index];
	{
		// Ideally this box shouldn't intersect more than 8 blocks if the box is cubic.
		const Box3i mip_blocks_box = box_in_voxels.downscaled(mip_data_lod.map.get_block_size() << top_lod_index);

		RWLockRead rlock(mip_data_lod.map_lock);

		const VoxelDataMap &map = mip_data_lod.map;
		const bool no_blocks_found = mip_blocks_box.all_cells_match([&map](const Vector3i pos) {
			const VoxelDataBlock *block = map.get_block(pos);
			return block == nullptr || block->has_voxels() == false;
		});

		if (no_blocks_found) {
			// No edits found at this mip, we may assume there are no edits in lower LODs.
			return false;
		}
	}

	// Assume there can be edits
	return true;
}

void VoxelData::view_area(Box3i blocks_box, std::vector<Vector3i> &missing_blocks,
		std::vector<Vector3i> &found_blocks_positions, std::vector<VoxelDataBlock> &found_blocks) {
	ZN_PROFILE_SCOPE();
	const Box3i bounds_in_blocks = get_bounds().downscaled(get_block_size());
	blocks_box = blocks_box.clipped(bounds_in_blocks);

	Lod &lod = _lods[0];
	RWLockRead rlock(lod.map_lock);

	blocks_box.for_each_cell_zxy([&lod, &found_blocks_positions, &found_blocks, &missing_blocks](Vector3i bpos) {
		VoxelDataBlock *block = lod.map.get_block(bpos);
		if (block != nullptr) {
			block->viewers.add();
			found_blocks.push_back(*block);
			found_blocks_positions.push_back(bpos);
		} else {
			missing_blocks.push_back(bpos);
		}
	});
}

void VoxelData::unview_area(Box3i blocks_box, std::vector<Vector3i> &missing_blocks,
		std::vector<Vector3i> &found_blocks, std::vector<BlockToSave> *to_save) {
	ZN_PROFILE_SCOPE();
	const Box3i bounds_in_blocks = get_bounds().downscaled(get_block_size());
	blocks_box = blocks_box.clipped(bounds_in_blocks);

	Lod &lod = _lods[0];
	RWLockRead rlock(lod.map_lock);

	blocks_box.for_each_cell_zxy([&lod, &missing_blocks, &found_blocks, to_save](Vector3i bpos) {
		VoxelDataBlock *block = lod.map.get_block(bpos);
		if (block != nullptr) {
			block->viewers.remove();
			if (block->viewers.get() == 0) {
				if (to_save == nullptr) {
					lod.map.remove_block(bpos, VoxelDataMap::NoAction());
				} else {
					lod.map.remove_block(bpos, BeforeUnloadSaveAction{ to_save, bpos, 0 });
				}
			}
			found_blocks.push_back(bpos);
		} else {
			missing_blocks.push_back(bpos);
		}
	});
}

std::shared_ptr<VoxelBufferInternal> VoxelData::try_get_block_voxels(Vector3i bpos) {
	Lod &lod = _lods[0];
	RWLockRead rlock(lod.map_lock);
	VoxelDataBlock *block = lod.map.get_block(bpos);
	if (block == nullptr) {
		return nullptr;
	}
	if (block->has_voxels()) {
		return block->get_voxels_shared();
	}
	return nullptr;
}

void VoxelData::set_voxel_metadata(Vector3i pos, Variant meta) {
	Lod &lod = _lods[0];
	RWLockRead rlock(lod.map_lock);
	VoxelDataBlock *block = lod.map.get_block(lod.map.voxel_to_block(pos));
	ZN_ASSERT_RETURN_MSG(block != nullptr, "Area not editable");
	// TODO Ability to have metadata in areas where voxels have not been allocated?
	// Otherwise we have to generate the block, because that's where it is stored at the moment.
	ZN_ASSERT_RETURN_MSG(block->has_voxels(), "Area not cached");
	RWLockWrite lock(block->get_voxels().get_lock());
	VoxelMetadata *meta_storage = block->get_voxels().get_or_create_voxel_metadata(lod.map.to_local(pos));
	ZN_ASSERT_RETURN(meta_storage != nullptr);
	gd::set_as_variant(*meta_storage, meta);
}

Variant VoxelData::get_voxel_metadata(Vector3i pos) {
	Lod &lod = _lods[0];
	RWLockRead rlock(lod.map_lock);
	VoxelDataBlock *block = lod.map.get_block(lod.map.voxel_to_block(pos));
	ZN_ASSERT_RETURN_V_MSG(block != nullptr, Variant(), "Area not editable");
	ZN_ASSERT_RETURN_V_MSG(block->has_voxels(), Variant(), "Area not cached");
	RWLockRead lock(block->get_voxels().get_lock());
	const VoxelMetadata *meta = block->get_voxels_const().get_voxel_metadata(lod.map.to_local(pos));
	if (meta == nullptr) {
		return Variant();
	}
	return gd::get_as_variant(*meta);
}

} // namespace zylann::voxel
