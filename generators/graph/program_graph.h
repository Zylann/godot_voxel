#ifndef ZN_PROGRAM_GRAPH_H
#define ZN_PROGRAM_GRAPH_H

#include "../../util/godot/core/variant.h"
#include "../../util/hash_funcs.h"
#include "../../util/math/vector2.h"
#include "../../util/non_copyable.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace zylann {

// Generic graph representing a program
class ProgramGraph : NonCopyable {
public:
	static const uint32_t NULL_ID = 0;
	static const uint32_t NULL_INDEX = -1;

	// TODO Use typedef to make things explicit
	// typedef uint32_t NodeID;

	struct PortLocation {
		uint32_t node_id;
		uint32_t port_index;
	};

	struct Connection {
		PortLocation src;
		PortLocation dst;
	};

	struct Port {
		std::vector<PortLocation> connections;
		// Dynamic ports are ports that are not inherited from `type_id`, they exist solely for this node.
		// Because it can't be deduced from `type_id`, they must be given a name.
		// Initially needed for expression nodes.
		std::string dynamic_name;
		uint32_t autoconnect_hint = 0;

		inline bool is_dynamic() const {
			return dynamic_name.size() > 0;
		}
	};

	struct Node {
		uint32_t id;
		uint32_t type_id;
		StringName name; // User-defined
		Vector2 gui_position;
		Vector2 gui_size; // Used on resizable nodes
		std::vector<Port> inputs;
		std::vector<Port> outputs;
		std::vector<Variant> params;
		std::vector<Variant> default_inputs;
		// When enabled, all disconnected inputs will automatically connect to a commonly used node when the graph is
		// compiled. If not enabled, default input values will be used instead.
		bool autoconnect_default_inputs = false;

		uint32_t find_input_connection(PortLocation src, uint32_t input_port_index) const;
		uint32_t find_output_connection(uint32_t output_port_index, PortLocation dst) const;

		bool find_input_port_by_name(std::string_view port_name, unsigned int &out_i) const;
	};

	~ProgramGraph();

	Node *create_node(uint32_t type_id, uint32_t id = NULL_ID);
	Node &get_node(uint32_t id) const;
	Node *try_get_node(uint32_t id) const;
	void remove_node(uint32_t id);
	void clear();

	bool is_connected(PortLocation src, PortLocation dst) const;

	// Checks if the specified connection can be created
	bool can_connect(PortLocation src, PortLocation dst) const;

	// Checks if the specified connection is valid (without considering existing connections)
	bool is_valid_connection(PortLocation src, PortLocation dst) const;

	void connect(PortLocation src, PortLocation dst);
	bool disconnect(PortLocation src, PortLocation dst);
	bool is_input_port_valid(PortLocation loc) const;
	bool is_output_port_valid(PortLocation loc) const;

	bool has_path(uint32_t p_src_node_id, uint32_t p_dst_node_id) const;
	void find_dependencies(uint32_t node_id, std::vector<uint32_t> &out_order) const;
	void find_dependencies(std::vector<uint32_t> nodes_to_process, std::vector<uint32_t> &out_order) const;
	void find_immediate_dependencies(uint32_t node_id, std::vector<uint32_t> &deps) const;
	void find_terminal_nodes(std::vector<uint32_t> &node_ids) const;

	template <typename F>
	inline void for_each_node_id(F f) const {
		for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
			f(it->first);
		}
	}

	template <typename F>
	inline void for_each_node_const(F f) const {
		for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
			const Node *node = it->second;
			ZN_ASSERT_CONTINUE(node != nullptr);
			f(*node);
		}
	}

	template <typename F>
	inline void for_each_node(F f) {
		for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
			Node *node = it->second;
			ZN_ASSERT_CONTINUE(node != nullptr);
			f(*node);
		}
	}

	template <typename F>
	inline uint32_t find_node(F f) const {
		for (auto it = _nodes.begin(); it != _nodes.end(); ++it) {
			const Node *node = it->second;
			ZN_ASSERT_CONTINUE(node != nullptr);
			if (f(*node)) {
				return it->first;
			}
		}
		return ProgramGraph::NULL_ID;
	}

	void copy_from(const ProgramGraph &other, bool copy_subresources);

	void get_connections(std::vector<ProgramGraph::Connection> &connections) const;
	void get_node_ids(std::vector<uint32_t> &node_ids) const;
	// void get_connections_from_and_to(std::vector<ProgramGraph::Connection> &connections, uint32_t node_id) const;

	// Finds first node having the given name and returns its ID. Returns NULL_ID if not found.
	uint32_t find_node_by_name(StringName name) const;
	// Finds first node having the given type and returns its ID. Returns NULL_ID if not found.
	uint32_t find_node_by_type(uint32_t type_id) const;

	uint32_t generate_node_id();

	unsigned int get_nodes_count() const;

	void debug_print_dot_file(String p_file_path) const;

private:
	std::unordered_map<uint32_t, Node *> _nodes;
	uint32_t _next_node_id = 1;
};

inline bool operator==(const ProgramGraph::PortLocation &a, const ProgramGraph::PortLocation &b) {
	return a.node_id == b.node_id && a.port_index == b.port_index;
}

// For Godot
struct ProgramGraphPortLocationHasher {
	static inline uint32_t hash(const ProgramGraph::PortLocation &v) {
		const uint32_t hash = hash_djb2_one_32(v.node_id);
		return hash_djb2_one_32(v.port_index, hash);
	}
};

} // namespace zylann

// For STL
namespace std {
template <>
struct hash<zylann::ProgramGraph::PortLocation> {
	size_t operator()(const zylann::ProgramGraph::PortLocation &v) const {
		return zylann::ProgramGraphPortLocationHasher::hash(v);
	}
};
} // namespace std

#endif // ZN_PROGRAM_GRAPH_H
