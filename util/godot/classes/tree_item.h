#ifndef ZN_GODOT_TREE_ITEM_H
#define ZN_GODOT_TREE_ITEM_H

#if defined(ZN_GODOT)
#include <scene/gui/tree.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/tree.hpp>
using namespace godot;
#endif

#include "node.h"

namespace zylann::godot {
namespace TreeItemUtilities {

inline void set_auto_translate_mode(TreeItem &item, const unsigned int column, const AutoTranslateMode mode) {
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4
	item.set_auto_translate_mode(column, to_godot_auto_translate_mode(mode));
#endif
}

} // namespace TreeItemUtilities
} // namespace zylann::godot

#endif // ZN_GODOT_TREE_ITEM_H
