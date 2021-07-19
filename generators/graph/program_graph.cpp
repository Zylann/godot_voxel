#include "program_graph.h"
#include <core/os/file_access.h>
#include <core/resource.h>
#include <core/variant.h>
#include <unordered_set>

template <typename T>
inline bool range_contains(const std::vector<T> &vec, const T &v, uint32_t begin, uint32_t end) {
	CRASH_COND(end > vec.size());
	for (size_t i = begin; i < end; ++i) {
		if (vec[i] == v) {
			return true;
		}
	}
	return false;
}

uint32_t ProgramGraph::Node::find_input_connection(PortLocation src, uint32_t input_port_index) const {
	CRASH_COND(input_port_index >= inputs.size());
	const Port &p = inputs[input_port_index];
	for (size_t i = 0; i < p.connections.size(); ++i) {
		if (p.connections[i] == src) {
			return i;
		}
	}
	return ProgramGraph::NULL_INDEX;
}

uint32_t ProgramGraph::Node::find_output_connection(uint32_t output_port_index, PortLocation dst) const {
	CRASH_COND(output_port_index >= outputs.size());
	const Port &p = outputs[output_port_index];
	for (size_t i = 0; i < p.connections.size(); ++i) {
		if (p.connections[i] == dst) {
			return i;
		}
	}
	return ProgramGraph::NULL_INDEX;
}

ProgramGraph::Node *ProgramGraph::create_node(uint32_t type_id, uint32_t id) {
	if (id == NULL_ID) {
		id = generate_node_id();
	} else {
		// ID must not be taken already
		ERR_FAIL_COND_V(_nodes.find(id) != _nodes.end(), nullptr);
		if (_next_node_id <= id) {
			_next_node_id = id + 1;
		}
	}
	Node *node = memnew(Node);
	node->id = id;
	node->type_id = type_id;
	_nodes[node->id] = node;
	return node;
}

void ProgramGraph::remove_node(uint32_t node_id) {
	Node *node = get_node(node_id);

	// Remove input connections
	for (uint32_t dst_port_index = 0; dst_port_index < node->inputs.size(); ++dst_port_index) {
		const Port &p = node->inputs[dst_port_index];
		for (auto it = p.connections.begin(); it != p.connections.end(); ++it) {
			const PortLocation src = *it;
			Node *src_node = get_node(src.node_id);
			uint32_t i = src_node->find_output_connection(src.port_index, PortLocation{ node_id, dst_port_index });
			CRASH_COND(i == NULL_INDEX);
			std::vector<PortLocation> &connections = src_node->outputs[src.port_index].connections;
			connections.erase(connections.begin() + i);
		}
	}

	// Remove output connections
	for (uint32_t src_port_index = 0; src_port_index < node->outputs.size(); ++src_port_index) {
		const Port &p = node->outputs[src_port_index];
		for (auto it = p.connections.begin(); it != p.connections.end(); ++it) {
			const PortLocation dst = *it;
			Node *dst_node = get_node(dst.node_id);
			uint32_t i = dst_node->find_input_connection(PortLocation{ node_id, src_port_index }, dst.port_index);
			CRASH_COND(i == NULL_INDEX);
			std::vector<PortLocation> &connections = dst_node->inputs[dst.port_index].connections;
			connections.erase(connections.begin() + i);
		}
	}

	_nodes.erase(node_id);
	memdelete(node);
}

void ProgramGraph::clear() {
	for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
		Node *node = it->second;
		CRASH_COND(node == nullptr);
		memdelete(node);
	}
	_nodes.clear();
}

bool ProgramGraph::is_connected(PortLocation src, PortLocation dst) const {
	Node *src_node = get_node(src.node_id);
	Node *dst_node = get_node(dst.node_id);
	if (src_node->find_output_connection(src.port_index, dst) != NULL_INDEX) {
		CRASH_COND(dst_node->find_input_connection(src, dst.port_index) == NULL_INDEX);
		return true;
	} else {
		CRASH_COND(dst_node->find_input_connection(src, dst.port_index) != NULL_INDEX);
		return false;
	}
}

bool ProgramGraph::can_connect(PortLocation src, PortLocation dst) const {
	if (is_connected(src, dst)) {
		// Already exists
		return false;
	}
	if (has_path(dst.node_id, src.node_id)) {
		// Would create a loop
		return false;
	}
	const Node *dst_node = get_node(dst.node_id);
	// There can be only one connection from a source to a destination
	return dst_node->inputs[dst.port_index].connections.size() == 0;
}

void ProgramGraph::connect(PortLocation src, PortLocation dst) {
	ERR_FAIL_COND(is_connected(src, dst));
	ERR_FAIL_COND(has_path(dst.node_id, src.node_id));
	Node *src_node = get_node(src.node_id);
	Node *dst_node = get_node(dst.node_id);
	ERR_FAIL_COND(dst_node->inputs[dst.port_index].connections.size() != 0);
	src_node->outputs[src.port_index].connections.push_back(dst);
	dst_node->inputs[dst.port_index].connections.push_back(src);
}

bool ProgramGraph::disconnect(PortLocation src, PortLocation dst) {
	Node *src_node = get_node(src.node_id);
	Node *dst_node = get_node(dst.node_id);
	uint32_t src_i = src_node->find_output_connection(src.port_index, dst);
	if (src_i == NULL_INDEX) {
		return false;
	}
	uint32_t dst_i = dst_node->find_input_connection(src, dst.port_index);
	CRASH_COND(dst_i == NULL_INDEX);
	std::vector<PortLocation> &src_connections = src_node->outputs[src.port_index].connections;
	std::vector<PortLocation> &dst_connections = dst_node->inputs[dst.port_index].connections;
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

ProgramGraph::Node *ProgramGraph::get_node(uint32_t id) const {
	auto it = _nodes.find(id);
	CRASH_COND(it == _nodes.end());
	Node *node = it->second;
	CRASH_COND(node == nullptr);
	return node;
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
		const Node *node = get_node(nodes_to_process.back());
		nodes_to_process.pop_back();
		visited_nodes.insert(node->id);

		uint32_t nodes_to_process_begin = nodes_to_process.size();

		// Find destinations
		for (uint32_t oi = 0; oi < node->outputs.size(); ++oi) {
			const Port &p = node->outputs[oi];
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

void ProgramGraph::find_dependencies(std::vector<uint32_t> nodes_to_process, std::vector<uint32_t> &out_order) const {
	// Finds dependencies of the given nodes, and returns them in the order they should be processed
	std::unordered_set<uint32_t> visited_nodes;

	while (nodes_to_process.size() > 0) {
		const Node *node = get_node(nodes_to_process.back());
		uint32_t nodes_to_process_begin = nodes_to_process.size();

		// Find ancestors
		for (uint32_t ii = 0; ii < node->inputs.size(); ++ii) {
			const Port &p = node->inputs[ii];
			for (auto cit = p.connections.begin(); cit != p.connections.end(); ++cit) {
				const PortLocation src = *cit;
				// A node can have two connections to the same destination node
				if (range_contains(nodes_to_process, src.node_id, nodes_to_process_begin, nodes_to_process.size())) {
					continue;
				}
				if (visited_nodes.find(src.node_id) != visited_nodes.end()) {
					continue;
				}
				nodes_to_process.push_back(src.node_id);
			}
		}

		if (nodes_to_process_begin == nodes_to_process.size()) {
			// No ancestor to visit, process the node
			out_order.push_back(node->id);
			visited_nodes.insert(node->id);
			nodes_to_process.pop_back();
		}
	}
}

void ProgramGraph::find_immediate_dependencies(uint32_t node_id, std::vector<uint32_t> &deps) const {
	const Node *node = get_node(node_id);
	const size_t begin = deps.size();

	for (uint32_t ii = 0; ii < node->inputs.size(); ++ii) {
		const Port &p = node->inputs[ii];

		for (auto cit = p.connections.begin(); cit != p.connections.end(); ++cit) {
			const PortLocation src = *cit;

			// A node can have two connections to the same destination node
			if (range_contains(deps, src.node_id, begin, deps.size())) {
				continue;
			}

			deps.push_back(src.node_id);
		}
	}
}

void ProgramGraph::find_depth_first(uint32_t start_node_id, std::vector<uint32_t> &order) const {
	// Finds each descendant from the given node, and returns them in the order they were found, depth-first.

	std::vector<uint32_t> nodes_to_process;
	std::unordered_set<uint32_t> visited_nodes;

	nodes_to_process.push_back(start_node_id);

	while (nodes_to_process.size() > 0) {
		const Node *node = get_node(nodes_to_process.back());
		nodes_to_process.pop_back();
		uint32_t nodes_to_process_begin = nodes_to_process.size();
		order.push_back(node->id);
		visited_nodes.insert(node->id);

		for (uint32_t oi = 0; oi < node->outputs.size(); ++oi) {
			const Port &p = node->outputs[oi];
			for (auto cit = p.connections.begin(); cit != p.connections.end(); ++cit) {
				PortLocation dst = *cit;
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
}

void ProgramGraph::debug_print_dot_file(String file_path) const {
	// https://www.graphviz.org/pdf/dotguide.pdf

	Error err;
	FileAccess *f = FileAccess::open(file_path, FileAccess::WRITE, &err);
	if (f == nullptr) {
		ERR_PRINT(String("Could not write ProgramGraph debug file as {0}: error {1}").format(varray(file_path, err)));
		return;
	}

	f->store_line("digraph G {");

	for (auto nit = _nodes.begin(); nit != _nodes.end(); ++nit) {
		const Node *node = nit->second;
		const uint32_t node_id = nit->first;

		for (auto oit = node->outputs.begin(); oit != node->outputs.end(); ++oit) {
			const Port &port = *oit;

			for (auto cit = port.connections.begin(); cit != port.connections.end(); ++cit) {
				PortLocation dst = *cit;
				f->store_line(String("\tn{0} -> n{1};").format(varray(node_id, dst.node_id)));
			}
		}
	}

	f->store_line("}");

	f->close();
	memdelete(f);
}

void ProgramGraph::copy_from(const ProgramGraph &other, bool copy_subresources) {
	clear();

	_next_node_id = other._next_node_id;
	_nodes.reserve(other._nodes.size());

	for (auto it = other._nodes.begin(); it != other._nodes.end(); ++it) {
		const Node *other_node = it->second;

		Node *node = memnew(Node);
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

uint32_t ProgramGraph::find_node_by_name(StringName name) const {
	for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
		const Node *node = it->second;
		if (node->name == name) {
			return it->first;
		}
	}
	return NULL_ID;
}

//void ProgramGraph::get_connections_from_and_to(std::vector<ProgramGraph::Connection> &connections, uint32_t node_id) const {
//	const Node *node = get_node(node_id);
//	ERR_FAIL_COND(node == nullptr);

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

PoolVector<int> ProgramGraph::get_node_ids() const {
	PoolIntArray ids;
	ids.resize(_nodes.size());
	{
		PoolIntArray::Write w = ids.write();
		int i = 0;
		for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
			w[i++] = it->first;
		}
	}
	return ids;
}

int ProgramGraph::get_nodes_count() const {
	return _nodes.size();
}

uint32_t ProgramGraph::generate_node_id() {
	return _next_node_id++;
}
