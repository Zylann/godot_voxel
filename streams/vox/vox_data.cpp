#include "vox_data.h"
#include "../../util/godot/classes/file.h"
#include "../../util/godot/core/array.h"
#include "../../util/log.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"

#include <unordered_set>

namespace zylann::voxel::magica {

const uint32_t PALETTE_SIZE = 256;

// clang-format off
uint32_t g_default_palette[PALETTE_SIZE] = {
	0x00000000, 0xffffffff, 0xffccffff, 0xff99ffff, 0xff66ffff, 0xff33ffff, 0xff00ffff, 0xffffccff,
	0xffccccff, 0xff99ccff, 0xff66ccff, 0xff33ccff, 0xff00ccff, 0xffff99ff, 0xffcc99ff, 0xff9999ff,
	0xff6699ff, 0xff3399ff, 0xff0099ff, 0xffff66ff, 0xffcc66ff, 0xff9966ff, 0xff6666ff, 0xff3366ff,
	0xff0066ff, 0xffff33ff, 0xffcc33ff, 0xff9933ff, 0xff6633ff, 0xff3333ff, 0xff0033ff, 0xffff00ff,
	0xffcc00ff, 0xff9900ff, 0xff6600ff, 0xff3300ff, 0xff0000ff, 0xffffffcc, 0xffccffcc, 0xff99ffcc,
	0xff66ffcc, 0xff33ffcc, 0xff00ffcc, 0xffffcccc, 0xffcccccc, 0xff99cccc, 0xff66cccc, 0xff33cccc,
	0xff00cccc, 0xffff99cc, 0xffcc99cc, 0xff9999cc, 0xff6699cc, 0xff3399cc, 0xff0099cc, 0xffff66cc,
	0xffcc66cc, 0xff9966cc, 0xff6666cc, 0xff3366cc, 0xff0066cc, 0xffff33cc, 0xffcc33cc, 0xff9933cc,
	0xff6633cc, 0xff3333cc, 0xff0033cc, 0xffff00cc, 0xffcc00cc, 0xff9900cc, 0xff6600cc, 0xff3300cc,
	0xff0000cc, 0xffffff99, 0xffccff99, 0xff99ff99, 0xff66ff99, 0xff33ff99, 0xff00ff99, 0xffffcc99,
	0xffcccc99, 0xff99cc99, 0xff66cc99, 0xff33cc99, 0xff00cc99, 0xffff9999, 0xffcc9999, 0xff999999,
	0xff669999, 0xff339999, 0xff009999, 0xffff6699, 0xffcc6699, 0xff996699, 0xff666699, 0xff336699,
	0xff006699, 0xffff3399, 0xffcc3399, 0xff993399, 0xff663399, 0xff333399, 0xff003399, 0xffff0099,
	0xffcc0099, 0xff990099, 0xff660099, 0xff330099, 0xff000099, 0xffffff66, 0xffccff66, 0xff99ff66,
	0xff66ff66, 0xff33ff66, 0xff00ff66, 0xffffcc66, 0xffcccc66, 0xff99cc66, 0xff66cc66, 0xff33cc66,
	0xff00cc66, 0xffff9966, 0xffcc9966, 0xff999966, 0xff669966, 0xff339966, 0xff009966, 0xffff6666,
	0xffcc6666, 0xff996666, 0xff666666, 0xff336666, 0xff006666, 0xffff3366, 0xffcc3366, 0xff993366,
	0xff663366, 0xff333366, 0xff003366, 0xffff0066, 0xffcc0066, 0xff990066, 0xff660066, 0xff330066,
	0xff000066, 0xffffff33, 0xffccff33, 0xff99ff33, 0xff66ff33, 0xff33ff33, 0xff00ff33, 0xffffcc33,
	0xffcccc33, 0xff99cc33, 0xff66cc33, 0xff33cc33, 0xff00cc33, 0xffff9933, 0xffcc9933, 0xff999933,
	0xff669933, 0xff339933, 0xff009933, 0xffff6633, 0xffcc6633, 0xff996633, 0xff666633, 0xff336633,
	0xff006633, 0xffff3333, 0xffcc3333, 0xff993333, 0xff663333, 0xff333333, 0xff003333, 0xffff0033,
	0xffcc0033, 0xff990033, 0xff660033, 0xff330033, 0xff000033, 0xffffff00, 0xffccff00, 0xff99ff00,
	0xff66ff00, 0xff33ff00, 0xff00ff00, 0xffffcc00, 0xffcccc00, 0xff99cc00, 0xff66cc00, 0xff33cc00,
	0xff00cc00, 0xffff9900, 0xffcc9900, 0xff999900, 0xff669900, 0xff339900, 0xff009900, 0xffff6600,
	0xffcc6600, 0xff996600, 0xff666600, 0xff336600, 0xff006600, 0xffff3300, 0xffcc3300, 0xff993300,
	0xff663300, 0xff333300, 0xff003300, 0xffff0000, 0xffcc0000, 0xff990000, 0xff660000, 0xff330000,
	0xff0000ee, 0xff0000dd, 0xff0000bb, 0xff0000aa, 0xff000088, 0xff000077, 0xff000055, 0xff000044,
	0xff000022, 0xff000011, 0xff00ee00, 0xff00dd00, 0xff00bb00, 0xff00aa00, 0xff008800, 0xff007700,
	0xff005500, 0xff004400, 0xff002200, 0xff001100, 0xffee0000, 0xffdd0000, 0xffbb0000, 0xffaa0000,
	0xff880000, 0xff770000, 0xff550000, 0xff440000, 0xff220000, 0xff110000, 0xffeeeeee, 0xffdddddd,
	0xffbbbbbb, 0xffaaaaaa, 0xff888888, 0xff777777, 0xff555555, 0xff444444, 0xff222222, 0xff111111
};
// clang-format on

static Error parse_string(FileAccess &f, String &s) {
	const int size = f.get_32();

	// Sanity checks
	ERR_FAIL_COND_V(size < 0, ERR_INVALID_DATA);
	ERR_FAIL_COND_V(size > 4096, ERR_INVALID_DATA);

	static thread_local std::vector<char> bytes;
	bytes.resize(size);
	ERR_FAIL_COND_V(
			get_buffer(f, Span<uint8_t>((uint8_t *)bytes.data(), bytes.size())) != bytes.size(), ERR_PARSE_ERROR);

	s = "";
	ERR_FAIL_COND_V(parse_utf8(s, to_span(bytes)) != OK, ERR_PARSE_ERROR);

	return OK;
}

static Error parse_dictionary(FileAccess &f, std::unordered_map<String, String> &dict) {
	const int item_count = f.get_32();

	// Sanity checks
	ERR_FAIL_COND_V(item_count < 0, ERR_INVALID_DATA);
	ERR_FAIL_COND_V(item_count > 256, ERR_INVALID_DATA);

	dict.clear();

	for (int i = 0; i < item_count; ++i) {
		String key;
		Error key_err = parse_string(f, key);
		ERR_FAIL_COND_V(key_err != OK, key_err);

		String value;
		Error value_err = parse_string(f, value);
		ERR_FAIL_COND_V(value_err != OK, value_err);

		dict[key] = value;
	}

	return OK;
}

// MagicaVoxel uses a Z-up coordinate system similar to 3DS Max.
// Here we read the data such that it follows OpenGL coordinate system.
//
//     Z             Y
//     | Y           | X
//     |/            |/
//     o----X        o----Z
//
//   MagicaVoxel     OpenGL
//
inline Vector3i magica_to_opengl(const Vector3i &src) {
	Vector3i dst;
	dst.x = src.y;
	dst.y = src.z;
	dst.z = src.x;
	return dst;
}

void transpose(Vector3i sx, Vector3i sy, Vector3i sz, Vector3i &dx, Vector3i &dy, Vector3i &dz) {
	dx.x = sx.x;
	dx.y = sy.x;
	dx.z = sz.x;

	dy.x = sx.y;
	dy.y = sy.y;
	dy.z = sz.y;

	dz.x = sx.z;
	dz.y = sy.z;
	dz.z = sz.z;
}

static Basis parse_basis(uint8_t data) {
	// bits 0 and 1 are the index of the non-zero entry in the first row
	const int xi = (data >> 0) & 0x03;
	// bits 2 and 3 are the index of the non-zero entry in the second row
	const int yi = (data >> 2) & 0x03;
	// The index of the non-zero entry in the last row can be deduced as the last not "occupied" index
	bool occupied[3] = { false };
	occupied[xi] = true;
	occupied[yi] = true;
	const int zi = occupied[0] == false ? 0 : occupied[1] == false ? 1 : 2;

	const int x_sign = ((data >> 4) & 0x01) == 0 ? 1 : -1;
	const int y_sign = ((data >> 5) & 0x01) == 0 ? 1 : -1;
	const int z_sign = ((data >> 6) & 0x01) == 0 ? 1 : -1;

	Vector3i x, y, z;
	x[xi] = x_sign;
	y[yi] = y_sign;
	z[zi] = z_sign;

	// The following is a bit messy, had a hard time figuring out the correct combination of conversions
	// to bring MagicaVoxel rotations to Godot rotations.
	// TODO Maybe this can be simplified?

	Vector3i magica_x, magica_y, magica_z;
	transpose(x, y, z, magica_x, magica_y, magica_z);
	// ZN_PRINT_VERBOSE(String("---\nX: {0}\nY: {1}\nZ: {2}")
	// 					  .format(varray(magica_x.to_vec3(), magica_y.to_vec3(), magica_z.to_vec3())));
	magica_x = magica_to_opengl(magica_x);
	magica_y = magica_to_opengl(magica_y);
	magica_z = magica_to_opengl(magica_z);
	z = magica_x;
	x = magica_y;
	y = magica_z;

	Basis b;
	b.set(x.x, y.x, z.x, x.y, y.y, z.y, x.z, y.z, z.z);

	return b;
}

Error parse_node_common_header(Node &node, FileAccess &f, const std::unordered_map<int, UniquePtr<Node>> &scene_graph) {
	//
	const int node_id = f.get_32();
	ERR_FAIL_COND_V_MSG(scene_graph.find(node_id) != scene_graph.end(), ERR_INVALID_DATA,
			String("Node with ID {0} already exists").format(varray(node_id)));

	node.id = node_id;

	const Error attributes_err = parse_dictionary(f, node.attributes);
	ERR_FAIL_COND_V(attributes_err != OK, attributes_err);

	return OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Data::clear() {
	_models.clear();
	_scene_graph.clear();
	_layers.clear();
	_materials.clear();
	_root_node_id = -1;
}

Error Data::load_from_file(String fpath) {
	const Error err = _load_from_file(fpath);
	if (err != OK) {
		clear();
	}
	return err;
}

Error Data::_load_from_file(String fpath) {
	ZN_PROFILE_SCOPE();
	// https://github.com/ephtracy/voxel-model/blob/master/MagicaVoxel-file-format-vox.txt
	// https://github.com/ephtracy/voxel-model/blob/master/MagicaVoxel-file-format-vox-extension.txt

	ZN_PRINT_VERBOSE(format("Loading {}", fpath));

	Error open_err;
	Ref<FileAccess> f_ref = open_file(fpath, FileAccess::READ, open_err);
	if (f_ref == nullptr) {
		return open_err;
	}
	FileAccess &f = **f_ref;

	char magic[5] = { 0 };
	ERR_FAIL_COND_V(get_buffer(f, Span<uint8_t>((uint8_t *)magic, 4)) != 4, ERR_PARSE_ERROR);
	ERR_FAIL_COND_V(strcmp(magic, "VOX ") != 0, ERR_PARSE_ERROR);

	const uint32_t version = f.get_32();
	ERR_FAIL_COND_V(version != 150, ERR_PARSE_ERROR);

	const size_t file_length = f.get_length();

	Vector3i last_size;

	clear();

	while (f.get_position() < file_length) {
		char chunk_id[5] = { 0 };
		ERR_FAIL_COND_V(get_buffer(f, Span<uint8_t>((uint8_t *)chunk_id, 4)) != 4, ERR_PARSE_ERROR);

		const uint32_t chunk_size = f.get_32();
		f.get_32(); // child_chunks_size

		ZN_PRINT_VERBOSE(format("Reading chunk {} at {}, size={}", chunk_id, f.get_position(), chunk_size));

		if (strcmp(chunk_id, "SIZE") == 0) {
			Vector3i size;
			size.x = f.get_32();
			size.y = f.get_32();
			size.z = f.get_32();
			ERR_FAIL_COND_V(size.x > 256, ERR_PARSE_ERROR);
			ERR_FAIL_COND_V(size.y > 256, ERR_PARSE_ERROR);
			ERR_FAIL_COND_V(size.z > 256, ERR_PARSE_ERROR);
			last_size = magica_to_opengl(size);

		} else if (strcmp(chunk_id, "XYZI") == 0) {
			UniquePtr<Model> model = make_unique_instance<Model>();
			model->color_indexes.resize(last_size.x * last_size.y * last_size.z, 0);
			model->size = last_size;

			const uint32_t num_voxels = f.get_32();

			for (uint32_t i = 0; i < num_voxels; ++i) {
				Vector3i pos;
				pos.x = f.get_8();
				pos.y = f.get_8();
				pos.z = f.get_8();
				const uint32_t c = f.get_8();
				pos = magica_to_opengl(pos);
				ERR_FAIL_COND_V(pos.x >= model->size.x || pos.x < 0, ERR_PARSE_ERROR);
				ERR_FAIL_COND_V(pos.y >= model->size.y || pos.y < 0, ERR_PARSE_ERROR);
				ERR_FAIL_COND_V(pos.z >= model->size.z || pos.z < 0, ERR_PARSE_ERROR);
				model->color_indexes[Vector3iUtil::get_zxy_index(pos, model->size)] = c;
			}

			_models.push_back(std::move(model));

		} else if (strcmp(chunk_id, "RGBA") == 0) {
			_palette[0] = Color8{ 0, 0, 0, 0 };
			for (uint32_t i = 1; i < _palette.size(); ++i) {
				Color8 c;
				c.r = f.get_8();
				c.g = f.get_8();
				c.b = f.get_8();
				c.a = f.get_8();
				_palette[i] = c;
			}
			f.get_32();

		} else if (strcmp(chunk_id, "nTRN") == 0) {
			UniquePtr<TransformNode> node_ptr = make_unique_instance<TransformNode>();
			TransformNode &node = *node_ptr;

			Error header_err = parse_node_common_header(node, f, _scene_graph);
			ERR_FAIL_COND_V(header_err != OK, header_err);

			auto name_it = node.attributes.find("_name");
			if (name_it != node.attributes.end()) {
				node.name = name_it->second;
			}

			auto hidden_it = node.attributes.find("_hidden");
			if (hidden_it != node.attributes.end()) {
				node.hidden = (hidden_it->second == "1");
			} else {
				node.hidden = false;
			}

			node.child_node_id = f.get_32();

			const int reserved_id = f.get_32();
			ERR_FAIL_COND_V(reserved_id != -1, ERR_INVALID_DATA);

			node.layer_id = f.get_32();

			const int frame_count = f.get_32();
			ERR_FAIL_COND_V(frame_count != 1, ERR_INVALID_DATA);

			// for (int frame_index = 0; frame_index < frame_count; ++frame_index) {

			std::unordered_map<String, String> frame;
			const Error frame_err = parse_dictionary(f, frame);
			ERR_FAIL_COND_V(frame_err != OK, frame_err);

			auto t_it = frame.find("_t");
			if (t_it != frame.end()) {
				// It is 3 integers formatted as text
				const PackedFloat64Array coords = t_it->second.split_floats(" ");
				ERR_FAIL_COND_V(coords.size() < 3, ERR_PARSE_ERROR);
				// ZN_PRINT_VERBOSE(String("Pos: {0}, {1}, {2}").format(varray(coords[0], coords[1], coords[2])));
				node.position = magica_to_opengl(Vector3i(coords[0], coords[1], coords[2]));
			}

			auto r_it = frame.find("_r");
			if (r_it != frame.end()) {
				Rotation rot;
				// TODO Is it really an integer formatted as text?
				rot.data = r_it->second.to_int();
				rot.basis = parse_basis(rot.data);
				node.rotation = rot;
			}

			//}

			_scene_graph[node.id] = std::move(node_ptr);

		} else if (strcmp(chunk_id, "nGRP") == 0) {
			UniquePtr<GroupNode> node_ptr = make_unique_instance<GroupNode>();
			GroupNode &node = *node_ptr;

			Error header_err = parse_node_common_header(node, f, _scene_graph);
			ERR_FAIL_COND_V(header_err != OK, header_err);

			const unsigned int child_count = f.get_32();
			// Sanity check
			ERR_FAIL_COND_V(child_count > 65536, ERR_INVALID_DATA);
			node.child_node_ids.resize(child_count);

			for (unsigned int i = 0; i < child_count; ++i) {
				node.child_node_ids[i] = f.get_32();
			}

			_scene_graph[node.id] = std::move(node_ptr);

		} else if (strcmp(chunk_id, "nSHP") == 0) {
			UniquePtr<ShapeNode> node_ptr = make_unique_instance<ShapeNode>();
			ShapeNode &node = *node_ptr;

			Error header_err = parse_node_common_header(node, f, _scene_graph);
			ERR_FAIL_COND_V(header_err != OK, header_err);

			const unsigned int model_count = f.get_32();
			ERR_FAIL_COND_V(model_count != 1, ERR_INVALID_DATA);

			// for (unsigned int i = 0; i < model_count; ++i) {

			node.model_id = f.get_32();
			ERR_FAIL_COND_V(node.model_id > 65536, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(node.model_id < 0, ERR_INVALID_DATA);

			Error model_attributes_err = parse_dictionary(f, node.model_attributes);
			ERR_FAIL_COND_V(model_attributes_err != OK, model_attributes_err);

			//}

			_scene_graph[node.id] = std::move(node_ptr);

		} else if (strcmp(chunk_id, "LAYR") == 0) {
			UniquePtr<Layer> layer_ptr = make_unique_instance<Layer>();
			Layer &layer = *layer_ptr;

			const int layer_id = f.get_32();
			for (unsigned int i = 0; i < _layers.size(); ++i) {
				const Layer *existing_layer = _layers[i].get();
				CRASH_COND(existing_layer == nullptr);
				ERR_FAIL_COND_V_MSG(existing_layer->id == layer_id, ERR_INVALID_DATA,
						String("Layer with ID {0} already exists").format(varray(layer_id)));
			}
			layer.id = layer_id;

			Error attributes_err = parse_dictionary(f, layer.attributes);
			ERR_FAIL_COND_V(attributes_err != OK, attributes_err);

			auto name_it = layer.attributes.find("_name");
			if (name_it != layer.attributes.end()) {
				layer.name = name_it->second;
			}

			auto hidden_it = layer.attributes.find("_hidden");
			if (hidden_it != layer.attributes.end()) {
				layer.hidden = (hidden_it->second == "1");
			} else {
				layer.hidden = false;
			}

			const int reserved_id = f.get_32();
			ERR_FAIL_COND_V(reserved_id != -1, ERR_INVALID_DATA);

			_layers.push_back(std::move(layer_ptr));

		} else if (strcmp(chunk_id, "MATL") == 0) {
			UniquePtr<Material> material_ptr = make_unique_instance<Material>();
			Material &material = *material_ptr;

			const int material_id = f.get_32();
			ERR_FAIL_COND_V(material_id < 0 || material_id > static_cast<int>(_palette.size()), ERR_INVALID_DATA);
			ERR_FAIL_COND_V_MSG(_materials.find(material_id) != _materials.end(), ERR_INVALID_DATA,
					String("Material ID {0} already exists").format(varray(material_id)));
			material.id = material_id;

			std::unordered_map<String, String> attributes;
			Error attributes_err = parse_dictionary(f, attributes);
			ERR_FAIL_COND_V(attributes_err != OK, attributes_err);

			auto type_it = attributes.find("_type");
			if (type_it != attributes.end()) {
				String type_name = type_it->second;
				if (type_name == "_diffuse") {
					material.type = Material::TYPE_DIFFUSE;
				} else if (type_name == "_metal") {
					material.type = Material::TYPE_METAL;
				} else if (type_name == "_glass") {
					material.type = Material::TYPE_GLASS;
				} else if (type_name == "_emit") {
					material.type = Material::TYPE_EMIT;
				}
			}

			auto weight_it = attributes.find("_weight");
			if (weight_it != attributes.end()) {
				material.weight = weight_it->second.to_float();
			}

			auto rough_it = attributes.find("_rough");
			if (rough_it != attributes.end()) {
				material.roughness = rough_it->second.to_float();
			}

			auto spec_it = attributes.find("_spec");
			if (spec_it != attributes.end()) {
				material.specular = spec_it->second.to_float();
			}

			auto ior_it = attributes.find("_ior");
			if (ior_it != attributes.end()) {
				material.ior = ior_it->second.to_float();
			}

			auto att_it = attributes.find("_att");
			if (att_it != attributes.end()) {
				material.att = att_it->second.to_float();
			}

			auto flux_it = attributes.find("_flux");
			if (flux_it != attributes.end()) {
				material.flux = flux_it->second.to_float();
			}

			// auto plastic_it = attributes.find("_plastic");
			// if (plastic_it != attributes.end()) {
			// 	// ???
			// }

			_materials.insert(std::make_pair(material_id, std::move(material_ptr)));

		} else {
			ZN_PRINT_VERBOSE(format("Skipping chunk {}", chunk_id));
			// Ignore chunk
			f.seek(f.get_position() + chunk_size);
		}
	}

	// There is no indication on the official spec to detect the root node of the scene graph.
	// It might just be the first one we find in the file, but the specification does not explicitly enforce that.
	// So we have to do it the long way, marking which nodes are referenced by others.
	std::unordered_set<int> referenced_nodes;

	// Validate scene graph
	for (auto it = _scene_graph.begin(); it != _scene_graph.end(); ++it) {
		const Node *node = it->second.get();
		CRASH_COND(node == nullptr);

		// TODO We should check for cycles too...

		switch (node->type) {
			case Node::TYPE_TRANSFORM: {
				const TransformNode *transform_node = reinterpret_cast<const TransformNode *>(node);

				const int child_id = transform_node->child_node_id;
				ERR_FAIL_COND_V_MSG(_scene_graph.find(child_id) == _scene_graph.end(), ERR_INVALID_DATA,
						String("Child node {0} does not exist").format(varray(child_id)));

				referenced_nodes.insert(child_id);

				const int layer_id = transform_node->layer_id;
				// Apparently it is possible to find nodes that are not attached to any layer
				if (layer_id != -1) {
					bool layer_exists = false;
					for (size_t i = 0; i < _layers.size(); ++i) {
						const Layer *layer = _layers[i].get();
						CRASH_COND(layer == nullptr);
						if (layer->id == layer_id) {
							layer_exists = true;
							break;
						}
					}
					ERR_FAIL_COND_V_MSG(!layer_exists, ERR_INVALID_DATA,
							String("Layer {0} does not exist").format(varray(layer_id)));
				}
			} break;

			case Node::TYPE_GROUP: {
				const GroupNode *group_node = reinterpret_cast<const GroupNode *>(node);
				for (size_t i = 0; i < group_node->child_node_ids.size(); ++i) {
					const int child_id = group_node->child_node_ids[i];
					ERR_FAIL_COND_V_MSG(_scene_graph.find(child_id) == _scene_graph.end(), ERR_INVALID_DATA,
							String("Child node {0} does not exist").format(varray(child_id)));
					referenced_nodes.insert(child_id);
				}
			} break;

			case Node::TYPE_SHAPE: {
				const ShapeNode *shape_node = reinterpret_cast<const ShapeNode *>(node);
				const int model_id = shape_node->model_id;
				ERR_FAIL_COND_V_MSG(model_id < 0 || model_id >= static_cast<int>(_models.size()), ERR_INVALID_DATA,
						String("Model {0} does not exist").format(varray(model_id)));
			} break;
		}
	}

	for (auto it = _scene_graph.begin(); it != _scene_graph.end(); ++it) {
		const int node_id = it->first;
		if (referenced_nodes.find(node_id) != referenced_nodes.end()) {
			continue;
		}
		ERR_FAIL_COND_V_MSG(_root_node_id != -1, ERR_INVALID_DATA, "More than one root node was found");
		_root_node_id = node_id;
	}

	// Some vox files don't have scene graph chunks
	if (_scene_graph.size() > 0) {
		// But if they do, they must have a root.
		// If this fails, that means there is a cycle (the opposite is not true)
		ERR_FAIL_COND_V_MSG(_root_node_id == -1, ERR_INVALID_DATA, "Root node not found");
	}

	ZN_PRINT_VERBOSE(format("Done loading {}", fpath));

	return OK;
}

unsigned int Data::get_model_count() const {
	return _models.size();
}

const Model &Data::get_model(unsigned int index) const {
	CRASH_COND(index >= _models.size());
	const Model *model = _models[index].get();
	CRASH_COND(model == nullptr);
	return *model;
}

const Node *Data::get_node(int id) const {
	auto it = _scene_graph.find(id);
	CRASH_COND(it == _scene_graph.end());
	const Node *node = it->second.get();
	CRASH_COND(node == nullptr);
	return node;
}

int Data::get_root_node_id() const {
	return _root_node_id;
}

unsigned int Data::get_layer_count() const {
	return _layers.size();
}

const Layer &Data::get_layer_by_index(unsigned int index) const {
	CRASH_COND(index >= _layers.size());
	const Layer *layer = _layers[index].get();
	CRASH_COND(layer == nullptr);
	return *layer;
}

int Data::get_material_id_for_palette_index(unsigned int palette_index) const {
	auto it = _materials.find(palette_index);
	if (it == _materials.end()) {
		return -1;
	}
	return it->first;
}

const Material &Data::get_material_by_id(int id) const {
	auto it = _materials.find(id);
	CRASH_COND(it == _materials.end());
	const Material *material = it->second.get();
	CRASH_COND(material == nullptr);
	return *material;
}

} // namespace zylann::voxel::magica
