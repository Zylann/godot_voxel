#ifndef VOX_DATA_H
#define VOX_DATA_H

#include "../../util/fixed_array.h"
#include "../../util/godot/core/string.h"
#include "../../util/math/basis.h"
#include "../../util/math/color8.h"
#include "../../util/math/vector3i.h"
#include "../../util/memory.h"

#if defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/global_constants.hpp> // For `Error`
#endif

#include <unordered_map>
#include <vector>

namespace zylann::voxel::magica {

struct Model {
	Vector3i size;
	// TODO Optimization: implement lazy loading/streaming to reduce intermediary memory allocations?
	// Loading a full 256^3 model needs 16 megabytes, but a lot of areas might actually be uniform,
	// and we might not need the actual model immediately
	std::vector<uint8_t> color_indexes;
};

struct Node {
	enum Type { //
		TYPE_TRANSFORM = 0,
		TYPE_GROUP,
		TYPE_SHAPE
	};

	int id;
	// Depending on the type, a node pointer can be casted to different structs
	const Type type;
	std::unordered_map<String, String> attributes;

	Node(Type p_type) : type(p_type) {}

	virtual ~Node() {}
};

struct Rotation {
	uint8_t data;
	Basis basis;
};

struct TransformNode : public Node {
	int child_node_id;
	int layer_id;
	// Pivot position, which turns out to be at the center in MagicaVoxel
	Vector3i position;
	Rotation rotation;
	String name;
	bool hidden;

	TransformNode() : Node(Node::TYPE_TRANSFORM) {}
};

struct GroupNode : public Node {
	std::vector<int> child_node_ids;

	GroupNode() : Node(Node::TYPE_GROUP) {}
};

struct ShapeNode : public Node {
	int model_id; // corresponds to index in the array of models
	std::unordered_map<String, String> model_attributes;

	ShapeNode() : Node(Node::TYPE_SHAPE) {}
};

struct Layer {
	int id;
	std::unordered_map<String, String> attributes;
	String name;
	bool hidden;
};

struct Material {
	enum Type { //
		TYPE_DIFFUSE,
		TYPE_METAL,
		TYPE_GLASS,
		TYPE_EMIT
	};
	int id;
	Type type = TYPE_DIFFUSE;
	float weight = 0.f;
	float roughness = 1.f;
	float specular = 0.f;
	float ior = 1.f; // refraction
	float flux = 0.f;
	// TODO I don't know what `_att` means
	float att = 0.f;
	// TODO WTF is `_plastic`?
};

class Data {
public:
	void clear();
	Error load_from_file(String fpath);

	unsigned int get_model_count() const;
	const Model &get_model(unsigned int index) const;

	// Can return -1 if there is no scene graph
	int get_root_node_id() const;
	const Node *get_node(int id) const;

	unsigned int get_layer_count() const;
	const Layer &get_layer_by_index(unsigned int index) const;

	int get_material_id_for_palette_index(unsigned int palette_index) const;
	const Material &get_material_by_id(int id) const;

	inline const FixedArray<Color8, 256> &get_palette() const {
		return _palette;
	}

private:
	Error _load_from_file(String fpath);

	std::vector<UniquePtr<Model>> _models;
	std::vector<UniquePtr<Layer>> _layers;
	std::unordered_map<int, UniquePtr<Node>> _scene_graph;
	// Material IDs are supposedly tied to palette indices
	std::unordered_map<int, UniquePtr<Material>> _materials;
	int _root_node_id = -1;
	FixedArray<Color8, 256> _palette;
};

} // namespace zylann::voxel::magica

#endif // VOX_DATA_H
