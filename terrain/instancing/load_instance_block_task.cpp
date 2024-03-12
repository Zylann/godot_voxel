#include "load_instance_block_task.h"
#include "../../engine/buffered_task_scheduler.h"
#include "../../streams/instance_data.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/classes/array_mesh.h"
#include "../../util/math/box3i.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "generate_instances_block_task.h"
#include "voxel_instancer_quick_reloading_cache.h"

namespace zylann::voxel {

LoadInstanceChunkTask::LoadInstanceChunkTask( //
		std::shared_ptr<VoxelInstancerTaskOutputQueue> output_queue, //
		Ref<VoxelStream> stream, //
		std::shared_ptr<VoxelInstancerQuickReloadingCache> quick_reload_cache,
		Ref<VoxelInstanceLibrary> library, //
		Array mesh_arrays, //
		Vector3i grid_position, //
		uint8_t lod_index, //
		uint8_t instance_block_size, //
		uint8_t data_block_size, //
		uint8_t up_mode //
		) :
		//
		_output_queue(output_queue), //
		_stream(stream), //
		_quick_reload_cache(quick_reload_cache), //
		_library(library), //
		_mesh_arrays(mesh_arrays), //
		_render_grid_position(grid_position), //
		_lod_index(lod_index), //
		_instance_block_size(instance_block_size), //
		_data_block_size(data_block_size), //
		_up_mode(up_mode) //
{
#ifdef DEBUG_ENABLED
	ZN_ASSERT(_output_queue != nullptr);
	ZN_ASSERT(_instance_block_size > 0);
	ZN_ASSERT(_data_block_size > 0);
	if (_instance_block_size < _data_block_size) {
		ZN_PRINT_ERROR("_instance_block_size < _data_block_size is not supported");
	}
#endif
}

void LoadInstanceChunkTask::run(ThreadedTaskContext &ctx) {
	ZN_PROFILE_SCOPE();

	struct Layer {
		int id = -1;
		uint8_t edited_mask = 0;
		StdVector<Transform3f> transforms;
	};

	StdVector<Layer> layers;

	// Try loading saved blocks
	if (_stream.is_valid()) {
		ZN_PROFILE_SCOPE();

		const unsigned int data_factor = _instance_block_size / _data_block_size;
		const Box3i data_box(_render_grid_position * data_factor, Vector3iUtil::create(data_factor));

		ZN_ASSERT(data_factor <= 2);
		FixedArray<VoxelStream::InstancesQueryData, 8> queries;

		// Create queries
		unsigned int query_count = 0;
		data_box.for_each_cell([&query_count, &queries, this](Vector3i data_pos) {
			VoxelStream::InstancesQueryData &query = queries[query_count];
			query.lod_index = _lod_index;
			query.position_in_blocks = data_pos;
			query.result = VoxelStream::RESULT_BLOCK_NOT_FOUND;
			++query_count;
		});

		{
			unsigned int stream_queries_count = 0;
			FixedArray<VoxelStream::InstancesQueryData, 8> stream_queries;
			FixedArray<uint8_t, 8> stream_queries_octant_indices;

			for (unsigned int i = 0; i < stream_queries_octant_indices.size(); ++i) {
				stream_queries_octant_indices[i] = i;
			}

			if (_quick_reload_cache != nullptr) {
				// Look first into quick reload cache and filter out queries to the stream... I don't like this,
				// especially the fact it gets complicated with differing chunk sizes
				MutexLock mlock(_quick_reload_cache->mutex);
				for (unsigned int query_index = 0; query_index < query_count; ++query_index) {
					VoxelStream::InstancesQueryData &query = queries[query_index];
					auto it = _quick_reload_cache->map.find(query.position_in_blocks);

					if (it != _quick_reload_cache->map.end()) {
						ZN_PROFILE_SCOPE_NAMED("Instance quick reload");
						UniquePtr<InstanceBlockData> data;
						if (it->second != nullptr) {
							data = make_unique_instance<InstanceBlockData>();
							it->second->copy_to(*data);
						}
						query.result = VoxelStream::RESULT_BLOCK_FOUND;
						query.data = std::move(data);

					} else {
						stream_queries[stream_queries_count] = std::move(query);
						stream_queries_octant_indices[stream_queries_count] = query_index;
						++stream_queries_count;
					}
				}

			} else {
				stream_queries = std::move(queries);
				stream_queries_count = query_count;
			}

			_stream->load_instance_blocks(to_span(stream_queries, stream_queries_count));

			for (unsigned int i = 0; i < stream_queries_count; ++i) {
				const unsigned int octant_index = stream_queries_octant_indices[i];
				queries[octant_index] = std::move(stream_queries[i]);
			}
		}

		const Vector3i data_min_block_pos = _render_grid_position * data_factor;

		// For each octant (will be only 1 if data chunks are the same size as render chunks, otherwise 8)
		// Populate layers from what we found in stream
		for (unsigned int octant_index = 0; octant_index < query_count; ++octant_index) {
			const VoxelStream::InstancesQueryData &query = queries[octant_index];

			if (query.result == VoxelStream::RESULT_BLOCK_FOUND) {
				if (query.data == nullptr) {
					// There must be no instances here at all, they were edited out

					if (query_count > 1) {
						for (Layer &layer : layers) {
							layer.edited_mask |= (1 << octant_index);
						}
					} else {
						for (Layer &layer : layers) {
							layer.edited_mask = 0xff;
						}
					}

					continue;
				}

				for (const InstanceBlockData::LayerData &loaded_layer_data : query.data->layers) {
					const int layer_id = loaded_layer_data.id;
					size_t layer_index;
					// Not using a hashmap here, this array is usually small
					if (!find(to_span_const(layers), layer_index,
								[layer_id](const Layer &layer) { return layer.id == layer_id; })) {
						layer_index = layers.size();
						Layer layer;
						layer.id = layer_id;
						layers.push_back(layer);
					}

					Layer &layer = layers[layer_index];

					if (query_count > 1) {
						// Mark octant as modified so the generator may skip it
						layer.edited_mask |= (1 << octant_index);
					} else {
						layer.edited_mask = 0xff;
					}

					const unsigned int dst_index0 = layer.transforms.size();

					layer.transforms.reserve(layer.transforms.size() + loaded_layer_data.instances.size());
					for (const InstanceBlockData::InstanceData &id : loaded_layer_data.instances) {
						layer.transforms.push_back(id.transform);
					}

					if (data_factor != 1) {
						// Data blocks store instances relative to a smaller grid than render blocks.
						// So we need to adjust their relative position.
						const Vector3f rel =
								to_vec3f((query.position_in_blocks - data_min_block_pos) * _data_block_size);
						ZN_ASSERT(dst_index0 <= layer.transforms.size());
						for (auto it = layer.transforms.begin() + dst_index0; it != layer.transforms.end(); ++it) {
							it->origin += rel;
						}
					}
				}
			}
		}
	}

	// Generate the rest
	if (_mesh_arrays.size() != 0 && _library.is_valid()) {
		ZN_PROFILE_SCOPE();

		// TODO Cache memory
		StdVector<VoxelInstanceLibrary::PackedItem> items;
		_library->get_packed_items_at_lod(items, _lod_index);

		if (items.size() > 0) {
			BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();

			for (const VoxelInstanceLibrary::PackedItem &item : items) {
				if (item.generator.is_valid()) {
					size_t layer_index;
					// Not using a hashmap here, this array is usually small
					const int layer_id = item.id;
					if (!find(to_span_const(layers), layer_index,
								[layer_id](const Layer &layer) { return layer.id == layer_id; })) {
						layer_index = layers.size();
						layers.push_back(Layer());
					}

					Layer &layer = layers[layer_index];
					if (layer.edited_mask == 0xff) {
						// Nothing to generate
						continue;
					}

					PackedVector3Array vertices = _mesh_arrays[ArrayMesh::ARRAY_VERTEX];

					if (vertices.size() != 0) {
						// Trigger a separate task because it may run in parallel, while the current one may not (until
						// we figure out how to make VoxelStream I/Os parallel enough)
						GenerateInstancesBlockTask *task = ZN_NEW(GenerateInstancesBlockTask);
						task->mesh_block_grid_position = _render_grid_position;
						task->layer_id = item.id;
						task->mesh_block_size = static_cast<int>(_instance_block_size) << _lod_index;
						task->lod_index = _lod_index;
						task->edited_mask = layer.edited_mask;
						task->up_mode = _up_mode;
						task->surface_arrays = _mesh_arrays;
						task->generator = item.generator;
						task->transforms = std::move(layer.transforms);
						task->output_queue = _output_queue;

						task_scheduler.push_main_task(task);
					}

					// We delegated the rest of loading to another task so we remove it from our list.
					layers[layer_index] = std::move(layers.back());
					layers.pop_back();
				}
			}

			task_scheduler.flush();
		}
	}

	// Post results
	for (Layer &layer : layers) {
		VoxelInstanceLoadingTaskOutput o;
		o.layer_id = layer.id;
		// Will normally be full
		o.edited_mask = layer.edited_mask;
		o.render_block_position = _render_grid_position;
		o.transforms = std::move(layer.transforms);
		{
			MutexLock(_output_queue->mutex);
			_output_queue->results.push_back(std::move(o));
		}
	}
}

} // namespace zylann::voxel
