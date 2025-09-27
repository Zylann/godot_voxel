#ifndef ZN_GODOT_GRAPH_EDIT_H
#define ZN_GODOT_GRAPH_EDIT_H

#if defined(ZN_GODOT)
#include <scene/gui/graph_edit.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/graph_edit.hpp>
using namespace godot;
#endif

#include "../../containers/std_vector.h"
#include "../core/string_name.h"
#include "../core/version.h"
#include "graph_edit_connection.h"

namespace zylann::godot {

void get_graph_edit_connections(const GraphEdit &self, StdVector<GraphEditConnection> &out_connections);
Vector2 get_graph_edit_scroll_offset(const GraphEdit &self);
bool is_graph_edit_using_snapping(const GraphEdit &self);
int get_graph_edit_snapping_distance(const GraphEdit &self);

GraphEditConnection get_graph_edit_closest_connection_at_point(
		const GraphEdit &self,
		const Vector2 point,
		const real_t max_distance = 4.0
);

} // namespace zylann::godot

#endif // ZN_GODOT_GRAPH_EDIT_H
