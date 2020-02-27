#ifndef PROGRAM_GRAPH_H
#define PROGRAM_GRAPH_H

#include <core/hashfuncs.h>
#include <unordered_map>
#include <vector>

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
		std::vector<Port> inputs;
		std::vector<Port> outputs;

		uint32_t find_input_connection(PortLocation src, uint32_t input_port_index) const;
		uint32_t find_output_connection(uint32_t output_port_index, PortLocation dst) const;
	};

	Node *create_node();
	Node *get_node(uint32_t id) const;
	void remove_node(uint32_t id);
	void clear();

	bool is_connected(PortLocation src, PortLocation dst) const;
	bool can_connect(PortLocation src, PortLocation dst) const;
	void connect(PortLocation src, PortLocation dst);
	bool disconnect(PortLocation src, PortLocation dst);

	bool has_path(uint32_t p_src_node_id, uint32_t p_dst_node_id) const;
	void find_dependencies(uint32_t end_node_id, std::vector<uint32_t> &order) const;
	void find_depth_first(uint32_t start_node_id, std::vector<uint32_t> &order) const;
	void find_terminal_nodes(std::vector<uint32_t> &node_ids) const;

	void copy_from(const ProgramGraph &other);
	void get_connections(std::vector<ProgramGraph::Connection> &connections) const;

	void debug_print_dot_file(String file_path) const;

private:
	std::unordered_map<uint32_t, Node *> _nodes;
	uint32_t _next_node_id = 1;
};

inline bool operator==(const ProgramGraph::PortLocation &a, const ProgramGraph::PortLocation &b) {
	return a.node_id == b.node_id && a.port_index == b.port_index;
}

#endif // PROGRAM_GRAPH_H
