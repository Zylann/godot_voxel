#include "blocky_material_indexer.h"
#include "../../util/godot/classes/material.h"
#include "../../util/string/format.h"
#include "blocky_baked_library.h"

namespace zylann::voxel::blocky {

unsigned int MaterialIndexer::get_or_create_index(const Ref<Material> &p_material) {
	for (size_t i = 0; i < materials.size(); ++i) {
		const Ref<Material> &material = materials[i];
		if (material == p_material) {
			return i;
		}
	}
#ifdef TOOLS_ENABLED
	if (materials.size() == MAX_MATERIALS) {
		ZN_PRINT_ERROR(
				format("Maximum material count reached ({}), try reduce your number of materials by re-using "
					   "them or using atlases.",
					   MAX_MATERIALS)
		);
	}
#endif
	const unsigned int ret = materials.size();
	materials.push_back(p_material);
	return ret;
}

} // namespace zylann::voxel::blocky
