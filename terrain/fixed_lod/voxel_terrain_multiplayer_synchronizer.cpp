#include "voxel_terrain_multiplayer_synchronizer.h"
#include "../../constants/voxel_string_names.h"
#include "../../streams/voxel_block_serializer.h"
#include "../../util/container_funcs.h"
#include "../../util/godot/classes/multiplayer_api.h"
#include "../../util/godot/classes/multiplayer_peer.h"
#include "../../util/godot/classes/scene_tree.h"
#include "../../util/godot/core/array.h"
#include "../../util/profiling.h"
#include "../../util/serialization.h"
#include "../../util/string_funcs.h"
#include "voxel_terrain.h"

namespace zylann::voxel {

VoxelTerrainMultiplayerSynchronizer::VoxelTerrainMultiplayerSynchronizer() {
	Dictionary config;
	config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	config["call_local"] = false;
	config["channel"] = _rpc_channel;

	rpc_config(VoxelStringNames::get_singleton()._rpc_receive_blocks, config);
	rpc_config(VoxelStringNames::get_singleton()._rpc_receive_area, config);

	set_process(true);
}

// Helper function
bool VoxelTerrainMultiplayerSynchronizer::is_server() const {
	ZN_ASSERT_RETURN_V(is_inside_tree(), false);
	// Note, when using multiple Multiplayer branches in the scene tree, `get_multiplayer` can be significantly slower,
	// because it involves constructing a NodePath and then querying a HashMap<NodePah,V> to check which nodes have
	// custom multiplayer. In cases Godot thought of, this is not a big issue, but in our case this can have a
	// noticeable impact because `is_server()` can be called a thousand times in a frame when a viewer joins or
	// teleports.
	Ref<MultiplayerAPI> mp = get_multiplayer();
	ZN_ASSERT_RETURN_V(mp.is_valid(), false);
	return mp->is_server();
}

void VoxelTerrainMultiplayerSynchronizer::send_block(
		int viewer_peer_id, const VoxelDataBlock &data_block, Vector3i bpos) {
	ZN_PROFILE_SCOPE();

	BlockSerializer::SerializeResult result = BlockSerializer::serialize_and_compress(data_block.get_voxels_const());
	ZN_ASSERT_RETURN(result.success);

	PackedByteArray data;
	data.resize(4 * sizeof(int16_t) + result.data.size());

	ByteSpanWithPosition mw_span(Span<uint8_t>(data.ptrw(), data.size()), 0);
	MemoryWriterExistingBuffer mw(mw_span, ENDIANESS_LITTLE_ENDIAN);

	mw.store_16(bpos.x);
	mw.store_16(bpos.y);
	mw.store_16(bpos.z);
	ZN_ASSERT_RETURN(result.data.size() <= 65535);
	mw.store_16(result.data.size());
	mw.store_buffer(to_span(result.data));

	// print_line(String("Server: send block {0}").format(varray(bpos)));

	// rpc_id(viewer_peer_id, VoxelStringNames::get_singleton().receive_block, data);
	// Instead of sending it right away, defer it until the terrain finished processing. Sending individual blocks with
	// the RPC system is too slow.
	_deferred_block_messages_per_peer[viewer_peer_id].push_back(DeferredBlockMessage{ data });
}

// TODO Have a way to implement ghost edits?
// If someone wants to spam edits to appear smooth on clients, this would have terrible impact on networking
// performance. So perhaps the server needs to cluster edits that are close together, and send the area in batch.
// Conversely, the client would have to apply the edit locally, while having a way to revert it if the server
// isn't acknowledging it for some time.

void VoxelTerrainMultiplayerSynchronizer::send_area(Box3i voxel_box) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(_terrain != nullptr);

	std::vector<ViewerID> viewers;
	_terrain->get_viewers_in_area(viewers, voxel_box);

	// Not particularly efficient for single-voxel edits, but should scale ok with bigger boxes
	VoxelBufferInternal voxels;
	voxels.create(voxel_box.size);
	_terrain->get_storage().copy(voxel_box.pos, voxels, 0xff);

	BlockSerializer::SerializeResult result = BlockSerializer::serialize_and_compress(voxels);
	ZN_ASSERT_RETURN(result.success);

	PackedByteArray pba;
	pba.resize(4 * sizeof(int32_t) + result.data.size());

	ByteSpanWithPosition mw_span(Span<uint8_t>(pba.ptrw(), pba.size()), 0);
	MemoryWriterExistingBuffer mw(mw_span, ENDIANESS_LITTLE_ENDIAN);

	mw.store_32(voxel_box.pos.x);
	mw.store_32(voxel_box.pos.y);
	mw.store_32(voxel_box.pos.z);
	mw.store_32(result.data.size());
	mw.store_buffer(to_span(result.data));

	for (const ViewerID viewer_id : viewers) {
		const int peer_id = VoxelEngine::get_singleton().get_viewer_network_peer_id(viewer_id);
		// TODO Don't bother copying and serializing if no networked viewers are around?
		if (peer_id != -1 && peer_id != MultiplayerPeer::TARGET_PEER_SERVER) {
			rpc_id(peer_id, VoxelStringNames::get_singleton()._rpc_receive_area, pba);
		}
	}
}

void VoxelTerrainMultiplayerSynchronizer::_notification(int p_what) {
	if (p_what == NOTIFICATION_PARENTED) {
		VoxelTerrain *terrain = Object::cast_to<VoxelTerrain>(get_parent());
		if (terrain != nullptr && terrain->get_multiplayer_synchronizer() == nullptr) {
			terrain->set_multiplayer_synchronizer(this);
			_terrain = terrain;
		}

	} else if (p_what == NOTIFICATION_UNPARENTED) {
		if (_terrain != nullptr && _terrain->get_multiplayer_synchronizer() == this) {
			_terrain->set_multiplayer_synchronizer(nullptr);
		}
		_terrain = nullptr;

	} else if (p_what == NOTIFICATION_PROCESS) {
		process();
	}
}

// template <typename T, typename F>
// inline void for_chunks(const std::vector<T> &vec, unsigned int chunk_size, F f) {
// 	for (unsigned int i = 0; i < vec.size(); i += chunk_size) {
// 		f(to_span_from_position_and_size(vec, i, i + chunk_size > vec.size() ? vec.size() - i : chunk_size));
// 	}
// }

void VoxelTerrainMultiplayerSynchronizer::process() {
	ZN_PROFILE_SCOPE();

	for (auto it = _deferred_block_messages_per_peer.begin(); it != _deferred_block_messages_per_peer.end(); ++it) {
		std::vector<DeferredBlockMessage> &messages = it->second;

		if (messages.size() == 0) {
			continue;
		}

		PackedByteArray pba;
		// Make one big fat message per frame per peer, because sending many is super-slow with Godot's ENet multiplayer
		// integration. It calls flush() on every RPC and that takes a lot of time, and there is overhead caused by
		// the high-level features...

		unsigned int size = 0;
		for (const DeferredBlockMessage &message : messages) {
			size += message.data.size();
		}

		pba.resize(1 * sizeof(uint32_t) + size);

		ByteSpanWithPosition mw_span(Span<uint8_t>(pba.ptrw(), pba.size()), 0);
		MemoryWriterExistingBuffer mw(mw_span, ENDIANESS_LITTLE_ENDIAN);
		mw.store_32(messages.size());

		for (const DeferredBlockMessage &message : messages) {
			mw.store_buffer(Span<const uint8_t>(message.data.ptr(), message.data.size()));
		}
		ZN_ASSERT(mw.data.size() == mw.data.pos);

		messages.clear();

		const int peer_id = it->first;
		ZN_PRINT_VERBOSE(format("Sending {} bytes of block data to peer {}", pba.size(), peer_id));
		// print_data_hex(Span<const uint8_t>(pba.ptr(), pba.size()));
		rpc_id(peer_id, VoxelStringNames::get_singleton()._rpc_receive_blocks, pba);
	}
}

void VoxelTerrainMultiplayerSynchronizer::_b_receive_blocks(PackedByteArray data) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(_terrain != nullptr);

	// print_line(String("Client: receive blocks data {1}").format(varray(data.size())));
	//  print_data_hex(Span<const uint8_t>(data.ptr(), data.size()));

	MemoryReader mr(Span<const uint8_t>(data.ptr(), data.size()), ENDIANESS_LITTLE_ENDIAN);

	const unsigned int block_count = mr.get_32();

	for (unsigned int i = 0; i < block_count; ++i) {
		Vector3i bpos;
		// This effectively limits volume size to 1,048,576. If really required, we could double this data to cover
		// more.
		bpos.x = int16_t(mr.get_16());
		bpos.y = int16_t(mr.get_16());
		bpos.z = int16_t(mr.get_16());
		const int voxel_data_size = mr.get_16();
		// print_line(String("Client: receive block {0} data {1}").format(varray(bpos, voxel_data_size)));

		VoxelBufferInternal voxels;
		ZN_ASSERT_RETURN(BlockSerializer::decompress_and_deserialize(mr.data.sub(mr.pos, voxel_data_size), voxels));

		mr.pos += voxel_data_size;

		std::shared_ptr<VoxelBufferInternal> voxels_p = make_shared_instance<VoxelBufferInternal>();
		*voxels_p = std::move(voxels);

		ZN_ASSERT_RETURN(_terrain != nullptr);
		_terrain->try_set_block_data(bpos, voxels_p);
	}
}

void VoxelTerrainMultiplayerSynchronizer::_b_receive_area(PackedByteArray data) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(_terrain != nullptr);

	MemoryReader mr(Span<const uint8_t>(data.ptr(), data.size()), ENDIANESS_LITTLE_ENDIAN);

	Vector3i pos;
	pos.x = int32_t(mr.get_32());
	pos.y = int32_t(mr.get_32());
	pos.z = int32_t(mr.get_32());
	const int voxel_data_size = mr.get_32();

	VoxelBufferInternal voxels;
	ZN_ASSERT_RETURN(BlockSerializer::decompress_and_deserialize(mr.data.sub(mr.pos, voxel_data_size), voxels));

	_terrain->get_storage().paste(pos, voxels, 0xff, false);
	_terrain->post_edit_area(Box3i(pos, voxels.get_size()));
}

#ifdef TOOLS_ENABLED

#if defined(ZN_GODOT)
PackedStringArray VoxelTerrainMultiplayerSynchronizer::get_configuration_warnings() const {
#elif defined(ZN_GODOT_EXTENSION)
PackedStringArray VoxelTerrainMultiplayerSynchronizer::_get_configuration_warnings() const {
#endif
	PackedStringArray warnings;

	if (is_inside_tree()) {
		if (_terrain == nullptr) {
			warnings.append(ZN_TTR("This node must be child of {0}").format(varray(VoxelTerrain::get_class_static())));
		}

		const Node *parent = get_parent();

		if (parent != nullptr) {
			const VoxelTerrain *terrain = Object::cast_to<VoxelTerrain>(parent);
			if (terrain != nullptr && terrain->get_multiplayer_synchronizer() != this) {
				warnings.append(ZN_TTR("Only one instance of {0} should exist under a {1}")
										.format(varray(VoxelTerrainMultiplayerSynchronizer::get_class_static(),
												VoxelTerrain::get_class_static())));
			}
		}
	}

	return warnings;
}

#endif

void VoxelTerrainMultiplayerSynchronizer::_bind_methods() {
	// TODO These methods are not supposed to be exposed. They only exist for Godot's high-level multiplayer to find
	// them.
	ClassDB::bind_method(
			D_METHOD("_rpc_receive_blocks", "data"), &VoxelTerrainMultiplayerSynchronizer::_b_receive_blocks);
	ClassDB::bind_method(D_METHOD("_rpc_receive_area", "data"), &VoxelTerrainMultiplayerSynchronizer::_b_receive_area);
}

} // namespace zylann::voxel
