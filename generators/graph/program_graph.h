#ifndef PROGRAM_GRAPH_H
#define PROGRAM_GRAPH_H

#include <core/hashfuncs.h>
#include <core/math/vector2.h>
#include <core/pool_vector.h>
#include <unordered_map>
#include <vector>

// Generic graph representing a program
class ProgramGraph {
public:
	static const uint32_t NULL_ID = 0;
	static const uint32_t NULL_INDEX = -1;

	struct PortLocation {
		uint32_t node_id;
		uint32_t port_index;
	};

	struct Connection {
		PortLocation src;
		PortLocation dst;
	};

	struct PortLocationHasher {
		static inline uint32_t hash(const PortLocation &v) {
			uint32_t hash = hash_djb2_one_32(v.node_id);
			return hash_djb2_one_32(v.node_id, hash);
		}
	};

	struct Port {
		std::vector<PortLocation> connections;
	};

	struct Node {
		uint32_t id;
		uint32_t type_id;
		StringName name; // User-defined
		Vector2 gui_position;
		std::vector<Port> inputs;
		std::vector<Port> outputs;
		std::vector<Variant> params;
		std::vector<Variant> default_inputs;

		uint32_t find_input_connection(PortLocation src, uint32_t input_port_index) const;
		uint32_t find_output_connection(uint32_t output_port_index, PortLocation dst) const;
	};

	~ProgramGraph();

	Node *create_node(uint32_t type_id, uint32_t id = NULL_ID);
	Node *get_node(uint32_t id) const;
	Node *try_get_node(uint32_t id) const;
	void remove_node(uint32_t id);
	void clear();

	bool is_connected(PortLocation src, PortLocation dst) const;
	bool can_connect(PortLocation src, PortLocation dst) const;
	void connect(PortLocation src, PortLocation dst);
	bool disconnect(PortLocation src, PortLocation dst);
	bool is_input_port_valid(PortLocation loc) const;
	bool is_output_port_valid(PortLocation loc) const;

	bool has_path(uint32_t p_src_node_id, uint32_t p_dst_node_id) const;
	void find_dependencies(std::vector<uint32_t> nodes_to_process, std::vector<uint32_t> &out_order) const;
	void find_immediate_dependencies(uint32_t node_id, std::vector<uint32_t> &deps) const;
	void find_depth_first(uint32_t start_node_id, std::vector<uint32_t> &order) const;
	void find_terminal_nodes(std::vector<uint32_t> &node_ids) const;

	template <typename F>
	inline void for_each_node_const(F f) const {
		for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
			const Node *node = it->second;
			ERR_CONTINUE(node == nullptr);
			f(*node);
		}
	}

	template <typename F>
	inline void for_each_node(F f) {
		for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
			Node *node = it->second;
			ERR_CONTINUE(node == nullptr);
			f(*node);
		}
	}

	void copy_from(const ProgramGraph &other, bool copy_subresources);
	void get_connections(std::vector<ProgramGraph::Connection> &connections) const;
	//void get_connections_from_and_to(std::vector<ProgramGraph::Connection> &connections, uint32_t node_id) const;

	uint32_t find_node_by_name(StringName name) const;

	PoolVector<int> get_node_ids() const;
	uint32_t generate_node_id();

	int get_nodes_count() const;

	void debug_print_dot_file(String file_path) const;

private:
	std::unordered_map<uint32_t, Node *> _nodes;
	uint32_t _next_node_id = 1;
};

inline bool operator==(const ProgramGraph::PortLocation &a, const ProgramGraph::PortLocation &b) {
	return a.node_id == b.node_id && a.port_index == b.port_index;
}

#endif // PROGRAM_GRAPH_H
