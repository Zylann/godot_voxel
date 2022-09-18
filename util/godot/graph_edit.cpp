#include "graph_edit.h"

namespace zylann {

void get_graph_edit_connections(const GraphEdit &self, std::vector<GodotGraphEditConnection> &out_connections) {
#if defined(ZN_GODOT)
#error "Implement get_graph_edit_connections for modules"
	// TODO
#elif defined(ZN_GODOT_EXTENSION)
	Array list = self.get_connection_list();
	const int count = list.size();

	const String from_key = "from";
	const String from_port_key = "from_port";
	const String to_key = "from";
	const String to_port_key = "from_port";

	for (int i = 0; i < count; ++i) {
		Dictionary d = list[i];
		GodotGraphEditConnection con;
		con.from = d[from_key];
		con.from_port = d[from_port_key];
		con.to = d[to_key];
		con.to_port = d[to_port_key];
		out_connections.push_back(con);
	}
#endif
}

} // namespace zylann
