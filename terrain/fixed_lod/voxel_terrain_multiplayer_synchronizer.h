#ifndef VOXEL_NETWORK_TERRAIN_SYNC_H
#define VOXEL_NETWORK_TERRAIN_SYNC_H

#include "../../storage/voxel_data_block.h"
#include "../../util/godot/classes/node.h"
#include <unordered_map>
#include <vector>

namespace zylann::voxel {

class VoxelTerrain;

// Implements multiplayer replication for `VoxelTerrain`
class VoxelTerrainMultiplayerSynchronizer : public Node {
	GDCLASS(VoxelTerrainMultiplayerSynchronizer, Node)
public:
	VoxelTerrainMultiplayerSynchronizer();

	bool is_server() const;

	void send_block(int viewer_peer_id, const VoxelDataBlock &data_block, Vector3i bpos);
	void send_area(Box3i voxel_box);

#ifdef TOOLS_ENABLED
#if defined(ZN_GODOT)
	PackedStringArray get_configuration_warnings() const override;
#elif defined(ZN_GODOT_EXTENSION)
	PackedStringArray _get_configuration_warnings() const override;
#endif
	void get_configuration_warnings(PackedStringArray &warnings) const;
#endif

private:
	void _notification(int p_what);

	void process();

	void _b_receive_blocks(PackedByteArray message_data);
	void _b_receive_area(PackedByteArray message_data);

	static void _bind_methods();

	VoxelTerrain *_terrain = nullptr;
	int _rpc_channel = 0;

	struct DeferredBlockMessage {
		PackedByteArray data;
	};

	std::unordered_map<int, std::vector<DeferredBlockMessage>> _deferred_block_messages_per_peer;
};

} // namespace zylann::voxel

#endif // VOXEL_NETWORK_TERRAIN_SYNC_H
