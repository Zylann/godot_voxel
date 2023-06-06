#include "program_graph.h"
#include "../../util/container_funcs.h"
#include "../../util/errors.h"
#include "../../util/godot/classes/resource.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/funcs.h"
#include "../../util/memory.h"
#include "../../util/string_funcs.h"

#include <fstream>
#include <unordered_set>

namespace zylann {

template <typename T>
inline bool range_contains(const std::vector<T> &vec, const T &v, uint32_t begin, uint32_t end) {
	ZN_ASSERT(end <= vec.size());
	for (size_t i = begin; i < end; ++i) {
		if (vec[i] == v) {
			return true;
		}
	}
	return false;
}

ProgramGraph::~ProgramGraph() {
	clear();
}

uint32_t ProgramGraph::Node::find_input_connection(PortLocation src, uint32_t input_port_index) const {
	ZN_ASSERT(input_port_index < inputs.size());
	const Port &p = inputs[input_port_index];
	for (size_t i = 0; i < p.connections.size(); ++i) {
		if (p.connections[i] == src) {
			return i;
		}
	}
	return ProgramGraph::NULL_INDEX;
}

uint32_t ProgramGraph::Node::find_output_connection(uint32_t output_port_index, PortLocation dst) const {
	ZN_ASSERT(output_port_index < outputs.size());
	const Port &p = outputs[output_port_index];
	for (size_t i = 0; i < p.connections.size(); ++i) {
		if (p.connections[i] == dst) {
			return i;
		}
	}
	return ProgramGraph::NULL_INDEX;
}

bool ProgramGraph::Node::find_input_port_by_name(std::string_view port_name, unsigned int &out_i) const {
	for (unsigned int i = 0; i < inputs.size(); ++i) {
		const ProgramGraph::Port &port = inputs[i];
		if (port.dynamic_name == port_name) {
			out_i = i;
			return true;
		}
	}
	return false;
}

ProgramGraph::Node *ProgramGraph::create_node(uint32_t type_id, uint32_t id) {
	if (id == NULL_ID) {
		id = generate_node_id();
	} else {
		// ID must not be taken already
		ZN_ASSERT_RETURN_V(_nodes.find(id) == _nodes.end(), nullptr);
		if (_next_node_id <= id) {
			_next_node_id = id + 1;
		}
	}
	Node *node = ZN_NEW(Node);
	node->id = id;
	node->type_id = type_id;
	_nodes[node->id] = node;
	return node;
}

void ProgramGraph::remove_node(uint32_t node_id) {
	Node &node = get_node(node_id);

	// Remove input connections
	for (uint32_t dst_port_index = 0; dst_port_index < node.inputs.size(); ++dst_port_index) {
		const Port &p = node.inputs[dst_port_index];
		for (auto it = p.connections.begin(); it != p.connections.end(); ++it) {
			const PortLocation src = *it;
			Node &src_node = get_node(src.node_id);
			const uint32_t i = src_node.find_output_connection(src.port_index, PortLocation{ node_id, dst_port_index });
			ZN_ASSERT(i != NULL_INDEX);
			std::vector<PortLocation> &connections = src_node.outputs[src.port_index].connections;
			connections.erase(connections.begin() + i);
		}
	}

	// Remove output connections
	for (uint32_t src_port_index = 0; src_port_index < node.outputs.size(); ++src_port_index) {
		const Port &p = node.outputs[src_port_index];
		for (auto it = p.connections.begin(); it != p.connections.end(); ++it) {
			const PortLocation dst = *it;
			Node &dst_node = get_node(dst.node_id);
			const uint32_t i = dst_node.find_input_connection(PortLocation{ node_id, src_port_index }, dst.port_index);
			ZN_ASSERT(i != NULL_INDEX);
			std::vector<PortLocation> &connections = dst_node.inputs[dst.port_index].connections;
			connections.erase(connections.begin() + i);
		}
	}

	_nodes.erase(node_id);
	ZN_DELETE(&node);
}

void ProgramGraph::clear() {
	for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
		Node *node = it->second;
		ZN_ASSERT(node != nullptr);
		ZN_DELETE(node);
	}
	_nodes.clear();
}

bool ProgramGraph::is_connected(PortLocation src, PortLocation dst) const {
	const Node &src_node = get_node(src.node_id);
	const Node &dst_node = get_node(dst.node_id);
	if (src_node.find_output_connection(src.port_index, dst) != NULL_INDEX) {
		ZN_ASSERT(dst_node.find_input_connection(src, dst.port_index) != NULL_INDEX);
		return true;
	} else {
		ZN_ASSERT(dst_node.find_input_connection(src, dst.port_index) == NULL_INDEX);
		return false;
	}
}

bool ProgramGraph::is_valid_connection(PortLocation src, PortLocation dst) const {
	if (src.node_id == dst.node_id) {
		// Can't connect to itself
		return false;
	}
	if (has_path(dst.node_id, src.node_id)) {
		// Would create a loop
		return false;
	}
	return true;
}

bool ProgramGraph::can_connect(PortLocation src, PortLocation dst) const {
	if (is_connected(src, dst)) {
		// Already exists
		return false;
	}
	if (!is_valid_connection(src, dst)) {
		return false;
	}
	const Node &dst_node = get_node(dst.node_id);
	// There can be only one connection from a source to a destination
	return dst_node.inputs[dst.port_index].connections.size() == 0;
}

void ProgramGraph::connect(PortLocation src, PortLocation dst) {
	ZN_ASSERT_RETURN_MSG(!is_connected(src, dst), "Cannot create the same connection twice.");
	ZN_ASSERT_RETURN_MSG(src.node_id != dst.node_id, "Cannot connect a node to itself.");
	ZN_ASSERT_RETURN_MSG(!has_path(dst.node_id, src.node_id), "Cannot add connection that would create a cycle.");
	Node &src_node = get_node(src.node_id);
	Node &dst_node = get_node(dst.node_id);
	ZN_ASSERT_RETURN_MSG(src.port_index < src_node.outputs.size(), "Source port doesn't exist");
	ZN_ASSERT_RETURN_MSG(dst.port_index < dst_node.inputs.size(), "Destination port doesn't exist");
	ZN_ASSERT_RETURN_MSG(
			dst_node.inputs[dst.port_index].connections.size() == 0, "Destination node's port is already connected");
	src_node.outputs[src.port_index].connections.push_back(dst);
	dst_node.inputs[dst.port_index].connections.push_back(src);
}

bool ProgramGraph::disconnect(PortLocation src, PortLocation dst) {
	Node &src_node = get_node(src.node_id);
	Node &dst_node = get_node(dst.node_id);
	const uint32_t src_i = src_node.find_output_connection(src.port_index, dst);
	if (src_i == NULL_INDEX) {
		return false;
	}
	const uint32_t dst_i = dst_node.find_input_connection(src, dst.port_index);
	ZN_ASSERT(dst_i != NULL_INDEX);
	std::vector<PortLocation> &src_connections = src_node.outputs[src.port_index].connections;
	std::vector<PortLocation> &dst_connections = dst_node.inputs[dst.port_index].connections;
	src_connections.erase(src_connections.begin() + src_i);
	dst_connections.erase(dst_connections.begin() + dst_i);
	return true;
}

bool ProgramGraph::is_input_port_valid(PortLocation loc) const {
	Node *node = try_get_node(loc.node_id);
	if (node == nullptr) {
		return false;
	}
	if (loc.port_index >= node->inputs.size()) {
		return false;
	}
	return true;
}

bool ProgramGraph::is_output_port_valid(PortLocation loc) const {
	Node *node = try_get_node(loc.node_id);
	if (node == nullptr) {
		return false;
	}
	if (loc.port_index >= node->outputs.size()) {
		return false;
	}
	return true;
}

ProgramGraph::Node &ProgramGraph::get_node(uint32_t id) const {
	auto it = _nodes.find(id);
	ZN_ASSERT(it != _nodes.end());
	Node *node = it->second;
	ZN_ASSERT(node != nullptr);
	return *node;
}

ProgramGraph::Node *ProgramGraph::try_get_node(uint32_t id) const {
	auto it = _nodes.find(id);
	if (it == _nodes.end()) {
		return nullptr;
	}
	return it->second;
}

bool ProgramGraph::has_path(uint32_t p_src_node_id, uint32_t p_dst_node_id) const {
	std::vector<uint32_t> nodes_to_process;
	std::unordered_set<uint32_t> visited_nodes;

	nodes_to_process.push_back(p_src_node_id);

	while (nodes_to_process.size() > 0) {
		const Node &node = get_node(nodes_to_process.back());
		nodes_to_process.pop_back();
		visited_nodes.insert(node.id);

		uint32_t nodes_to_process_begin = nodes_to_process.size();

		// Find destinations
		for (uint32_t oi = 0; oi < node.outputs.size(); ++oi) {
			const Port &p = node.outputs[oi];
			for (auto cit = p.connections.begin(); cit != p.connections.end(); ++cit) {
				PortLocation dst = *cit;
				if (dst.node_id == p_dst_node_id) {
					return true;
				}
				// A node can have two connections to the same destination node
				if (range_contains(nodes_to_process, dst.node_id, nodes_to_process_begin, nodes_to_process.size())) {
					continue;
				}
				if (visited_nodes.find(dst.node_id) != visited_nodes.end()) {
					continue;
				}
				nodes_to_process.push_back(dst.node_id);
			}
		}
	}

	return false;
}

void ProgramGraph::find_terminal_nodes(std::vector<uint32_t> &node_ids) const {
	for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
		const Node *node = it->second;
		if (node->outputs.size() == 0) {
			node_ids.push_back(it->first);
		}
	}
}

void ProgramGraph::find_dependencies(uint32_t node_id, std::vector<uint32_t> &out_order) const {
	std::vector<uint32_t> nodes_to_process;
	nodes_to_process.push_back(node_id);
	find_dependencies(nodes_to_process, out_order);
}

// Finds dependencies of the given nodes, and returns them in the order they should be processed.
// Given nodes are included in the result.
void ProgramGraph::find_dependencies(std::vector<uint32_t> nodes_to_process, std::vector<uint32_t> &out_order) const {
	std::unordered_set<uint32_t> visited_nodes;

	while (nodes_to_process.size() > 0) {
	found:
		// The loop can come back multiple times to the same node, until all its dependencies have been processed.
		const Node &node = get_node(nodes_to_process.back());
		ZN_ASSERT_MSG(nodes_to_process.size() <= get_nodes_count(), "Invalid graph?");

		// Pick first non-visited dependency
		for (const Port &port : node.inputs) {
			for (const PortLocation src : port.connections) {
				if (visited_nodes.find(src.node_id) == visited_nodes.end()) {
					nodes_to_process.push_back(src.node_id);
					goto found;
				}
			}
		}

		// No dependencies left to visit, process node
		out_order.push_back(node.id);
		visited_nodes.insert(node.id);
		nodes_to_process.pop_back();
	}

#if DEBUG_ENABLED
	ZN_ASSERT(!has_duplicate(to_span_const(out_order)));
#endif
}

void ProgramGraph::find_immediate_dependencies(uint32_t node_id, std::vector<uint32_t> &deps) const {
	const Node &node = get_node(node_id);
	const size_t begin = deps.size();

	for (const Port &p : node.inputs) {
		for (const PortLocation src : p.connections) {
			// A node can have two connections to the same destination node
			if (range_contains(deps, src.node_id, begin, deps.size())) {
				continue;
			}

			deps.push_back(src.node_id);
		}
	}
}

void ProgramGraph::debug_print_dot_file(String p_file_path) const {
	// https://www.graphviz.org/pdf/dotguide.pdf

	const std::string file_path = to_std_string(p_file_path);
	std::ofstream ofs(file_path, std::ios::binary | std::ios::trunc | std::ios::out);

	if (!ofs.good()) {
		ZN_PRINT_VERBOSE(format("Could not write ProgramGraph debug file as {}", file_path));
		return;
	}

	ofs << "digraph G {\n";

	for (auto nit = _nodes.begin(); nit != _nodes.end(); ++nit) {
		const Node *node = nit->second;
		const uint32_t node_id = nit->first;

		for (auto oit = node->outputs.begin(); oit != node->outputs.end(); ++oit) {
			const Port &port = *oit;

			for (auto cit = port.connections.begin(); cit != port.connections.end(); ++cit) {
				PortLocation dst = *cit;
				ofs << format("\tn{} -> n{};\n", node_id, dst.node_id);
			}
		}
	}

	ofs << "}\n";
	ofs.close();
}

void ProgramGraph::copy_from(const ProgramGraph &other, bool copy_subresources) {
	clear();

	_next_node_id = other._next_node_id;
	_nodes.reserve(other._nodes.size());

	for (auto it = other._nodes.begin(); it != other._nodes.end(); ++it) {
		const Node *other_node = it->second;

		Node *node = ZN_NEW(Node);
		*node = *other_node;

		if (copy_subresources) {
			for (size_t i = 0; i < node->params.size(); ++i) {
				Object *obj = node->params[i];
				if (obj != nullptr) {
					Resource *res = Object::cast_to<Resource>(obj);
					if (res != nullptr) {
						node->params[i] = res->duplicate(copy_subresources);
					}
				}
			}
		}

		// IDs should be the same
		_nodes.insert(std::make_pair(node->id, node));
	}
}

void ProgramGraph::get_connections(std::vector<ProgramGraph::Connection> &connections) const {
	for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
		const Node *node = it->second;

		for (size_t i = 0; i < node->outputs.size(); ++i) {
			const Port &port = node->outputs[i];

			for (size_t j = 0; j < port.connections.size(); ++j) {
				Connection con;
				con.src = PortLocation{ node->id, uint32_t(i) };
				con.dst = port.connections[j];
				connections.push_back(con);
			}
		}
	}
}

void ProgramGraph::get_node_ids(std::vector<uint32_t> &node_ids) const {
	ZN_ASSERT(node_ids.size() == 0);
	node_ids.reserve(node_ids.size());
	for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
		node_ids.push_back(it->first);
	}
}

uint32_t ProgramGraph::find_node_by_name(StringName name) const {
	for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
		const Node *node = it->second;
		if (node->name == name) {
			return it->first;
		}
	}
	return NULL_ID;
}

uint32_t ProgramGraph::find_node_by_type(uint32_t type_id) const {
	for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
		const Node *node = it->second;
		if (node->type_id == type_id) {
			return it->first;
		}
	}
	return NULL_ID;
}

// void ProgramGraph::get_connections_from_and_to(std::vector<ProgramGraph::Connection> &connections, uint32_t node_id)
// const { 	const Node *node = get_node(node_id); 	ERR_FAIL_COND(node == nullptr);

//	for (size_t i = 0; i < node->outputs.size(); ++i) {
//		const Port &port = node->outputs[i];

//		for (size_t j = 0; j < port.connections.size(); ++j) {
//			Connection con;
//			con.src = PortLocation{ node->id, uint32_t(i) };
//			con.dst = port.connections[j];
//			connections.push_back(con);
//		}
//	}

//	for (size_t i = 0; i < node->inputs.size(); ++i) {
//		const Port &port = node->inputs[i];

//		for (size_t j = 0; j < port.connections.size(); ++j) {
//			Connection con;
//			con.src = PortLocation{ node->id, uint32_t(i) };
//			con.dst = port.connections[j];
//			connections.push_back(con);
//		}
//	}
//}

unsigned int ProgramGraph::get_nodes_count() const {
	return _nodes.size();
}

uint32_t ProgramGraph::generate_node_id() {
	return _next_node_id++;
}

} // namespace zylann
