#ifndef ZN_GODOT_GRAPH_EDIT_CONNECTION_H
#define ZN_GODOT_GRAPH_EDIT_CONNECTION_H

#include "../core/string_name.h"

namespace zylann::godot {

struct GraphEditConnection {
	StringName from;
	StringName to;
	int from_port = 0;
	int to_port = 0;
	// float activity = 0.0;

	inline bool is_valid() const {
		return !from.is_empty();
	}
};

} // namespace zylann::godot

#endif // ZN_GODOT_GRAPH_EDIT_CONNECTION_H
