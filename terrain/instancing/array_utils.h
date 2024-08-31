#ifndef ZN_GODOT_ARRAY_UTILS_H
#define ZN_GODOT_ARRAY_UTILS_H

#include "../../util/containers/container_funcs.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/core/typed_array.h"
#include "../../util/string/format.h"

namespace zylann::voxel {

// Godot will assign array properties entirely regardless of what the change was (adding one item, removing one
// item, changing an item's position, multiple items, or setting a whole new array). So we have to diff the previous
// and new array to find out what has been removed and what has been added.
// In addition, we have to ignore null entries because it can happen when users mess around in the editor...
template <typename TResource>
void diff_set_array(
		StdVector<Ref<TResource>> &dst,
		const TypedArray<TResource> &src,
		StdVector<Ref<TResource>> &added_items,
		StdVector<Ref<TResource>> &removed_items
) {
	StdVector<Ref<TResource>> new_items;
	zylann::godot::copy_to(new_items, src);

	// Disallow duplicates.
	// We don't support that because it prevents us from identifying them properly when diffing the array...
	while (true) {
		const DuplicateSearchResult res = find_duplicate_ex(to_span_const(new_items), Ref<TResource>());
		if (res.is_null()) {
			break;
		}
		ZN_PRINT_ERROR(
				format("Duplicate {} is not allowed (at indices {} and {})",
					   String(TResource::get_class_static()),
					   res.first,
					   res.second)
		);
		new_items[res.second] = Ref<TResource>();
	}

	for (unsigned int i = 0; i < new_items.size(); ++i) {
		const Ref<TResource> &new_item = new_items[i];
		if (new_item.is_null()) {
			continue;
		}
		if (!contains(dst, new_item)) {
			added_items.push_back(new_item);
		}
	}

	for (unsigned int i = 0; i < dst.size(); ++i) {
		const Ref<TResource> &old_item = dst[i];
		if (old_item.is_null()) {
			continue;
		}
		if (!contains(dst, old_item)) {
			removed_items.push_back(old_item);
		}
	}

	dst = std::move(new_items);
}

} // namespace zylann::voxel

#endif // ZN_GODOT_ARRAY_UTILS_H
