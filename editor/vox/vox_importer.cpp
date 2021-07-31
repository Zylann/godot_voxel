#include "vox_importer.h"
#include "../../meshers/cubes/voxel_mesher_cubes.h"
#include "../../storage/voxel_buffer.h"
#include "../../streams/vox_data.h"

#include <scene/3d/mesh_instance.h>
#include <scene/3d/spatial.h>
#include <scene/resources/mesh.h>
#include <scene/resources/packed_scene.h>

String VoxelVoxImporter::get_importer_name() const {
	return "VoxelVoxImporter";
}

String VoxelVoxImporter::get_visible_name() const {
	return "VoxelVoxImporter";
}

void VoxelVoxImporter::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("vox");
}

String VoxelVoxImporter::get_preset_name(int p_idx) const {
	return "Default";
}

int VoxelVoxImporter::get_preset_count() const {
	return 1;
}

String VoxelVoxImporter::get_save_extension() const {
	return "tscn";
}

String VoxelVoxImporter::get_resource_type() const {
	return "PackedScene";
}

// float VoxelVoxImporter::get_priority() const {
//     return 0;
// }

// int VoxelVoxImporter::get_import_order() const {
// }

void VoxelVoxImporter::get_import_options(List<ImportOption> *r_options, int p_preset) const {
}

bool VoxelVoxImporter::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {
	return false;
}

static void add_mesh_instance(Ref<Mesh> mesh, Node *parent, Node *owner, Vector3 offset) {
	MeshInstance *mesh_instance = memnew(MeshInstance);
	mesh_instance->set_mesh(mesh);
	parent->add_child(mesh_instance);
	mesh_instance->set_owner(owner);
	mesh_instance->set_translation(offset);
	// TODO Colliders? Needs conventions or attributes probably
}

struct VoxMesh {
	Ref<Mesh> mesh;
	Vector3 pivot;
};

static Error process_scene_node_recursively(const vox::Data &data, int node_id, Spatial *parent_node,
		Spatial *&root_node, int depth, const Vector<VoxMesh> &meshes) {
	//
	ERR_FAIL_COND_V(depth > 10, ERR_INVALID_DATA);
	const vox::Node *vox_node = data.get_node(node_id);

	switch (vox_node->type) {
		case vox::Node::TYPE_TRANSFORM: {
			Spatial *node = memnew(Spatial);
			if (root_node == nullptr) {
				root_node = node;
			} else {
				ERR_FAIL_COND_V(parent_node == nullptr, ERR_BUG);
				parent_node->add_child(node);
				node->set_owner(root_node);
			}
			const vox::TransformNode *vox_transform_node = reinterpret_cast<const vox::TransformNode *>(vox_node);
			node->set_transform(Transform(vox_transform_node->rotation.basis, vox_transform_node->position.to_vec3()));
			process_scene_node_recursively(data, vox_transform_node->child_node_id, node, root_node, depth + 1, meshes);
		} break;

		case vox::Node::TYPE_GROUP: {
			const vox::GroupNode *vox_group_node = reinterpret_cast<const vox::GroupNode *>(vox_node);
			for (unsigned int i = 0; i < vox_group_node->child_node_ids.size(); ++i) {
				const int child_node_id = vox_group_node->child_node_ids[i];
				process_scene_node_recursively(data, child_node_id, parent_node, root_node, depth + 1, meshes);
			}
		} break;

		case vox::Node::TYPE_SHAPE: {
			ERR_FAIL_COND_V(parent_node == nullptr, ERR_BUG);
			ERR_FAIL_COND_V(root_node == nullptr, ERR_BUG);
			const vox::ShapeNode *vox_shape_node = reinterpret_cast<const vox::ShapeNode *>(vox_node);
			const VoxMesh &mesh_data = meshes[vox_shape_node->model_id];
			ERR_FAIL_COND_V(mesh_data.mesh.is_null(), ERR_BUG);
			const Vector3 offset = -mesh_data.pivot;
			add_mesh_instance(mesh_data.mesh, parent_node, root_node, offset);
		} break;

		default:
			ERR_FAIL_V(ERR_INVALID_DATA);
			break;
	}

	return OK;
}

Error VoxelVoxImporter::import(const String &p_source_file, const String &p_save_path,
		const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files,
		Variant *r_metadata) {
	//
	vox::Data data;
	const Error load_err = data.load_from_file(p_source_file);
	ERR_FAIL_COND_V(load_err != OK, load_err);

	Vector<VoxMesh> meshes;
	meshes.resize(data.get_model_count());

	// Get color palette
	Ref<VoxelColorPalette> palette;
	palette.instance();
	for (unsigned int i = 0; i < data.get_palette().size(); ++i) {
		Color8 color = data.get_palette()[i];
		palette->set_color8(i, color);
	}

	Ref<VoxelMesherCubes> mesher;
	mesher.instance();
	mesher->set_color_mode(VoxelMesherCubes::COLOR_MESHER_PALETTE);
	mesher->set_palette(palette);
	mesher->set_greedy_meshing_enabled(true);

	Ref<SpatialMaterial> material;
	material.instance();
	material->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	material->set_roughness(1.f);

	Ref<SpatialMaterial> material2;
	material2.instance();
	material2->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	material2->set_roughness(1.f);

	Array materials_array;
	materials_array.push_back(material);
	materials_array.push_back(material2);

	// Build meshes from voxel models
	for (unsigned int i = 0; i < data.get_model_count(); ++i) {
		const vox::Model &model = data.get_model(i);

		Ref<VoxelBuffer> voxels;
		voxels.instance();
		voxels->create(model.size + Vector3i(VoxelMesherCubes::PADDING * 2));
		voxels->decompress_channel(VoxelBuffer::CHANNEL_COLOR);

		Span<uint8_t> dst_color_indices;
		ERR_FAIL_COND_V(!voxels->get_channel_raw(VoxelBuffer::CHANNEL_COLOR, dst_color_indices), ERR_BUG);
		Span<const uint8_t> src_color_indices = to_span_const(model.color_indexes);
		copy_3d_region_zxy(dst_color_indices, voxels->get_size(), Vector3i(VoxelMesherCubes::PADDING),
				src_color_indices, model.size, Vector3i(), model.size);

		Ref<Mesh> mesh = mesher->build_mesh(voxels, materials_array);
		VoxMesh mesh_info;
		mesh_info.mesh = mesh;
		// In MagicaVoxel scene graph, pivots are at the center of models, not at the lower corner.
		// TODO I don't know if this is correct, but I could not find a reference saying how that pivot should be calculated
		mesh_info.pivot = (voxels->get_size() / 2 - Vector3i(1)).to_vec3();
		meshes.write[i] = mesh_info;
	}

	Spatial *root_node = nullptr;
	if (data.get_root_node_id() != -1) {
		// Convert scene graph into a node tree
		process_scene_node_recursively(data, data.get_root_node_id(), nullptr, root_node, 0, meshes);
		ERR_FAIL_COND_V(root_node == nullptr, ERR_INVALID_DATA);

	} else if (meshes.size() > 0) {
		// Some vox files don't have a scene graph
		root_node = memnew(Spatial);
		const VoxMesh &mesh0 = meshes[0];
		add_mesh_instance(mesh0.mesh, root_node, root_node, Vector3());
	}

	// Save meshes
	for (int i = 0; i < meshes.size(); ++i) {
		Ref<Mesh> mesh = meshes[i].mesh;
		String res_save_path = String("{0}.model{1}.mesh").format(varray(p_save_path, i));
		ResourceSaver::save(res_save_path, mesh);
	}

	root_node->set_name(p_save_path.get_file().get_basename());

	// Save scene
	Ref<PackedScene> scene;
	scene.instance();
	scene->pack(root_node);
	String scene_save_path = p_save_path + ".tscn";
	Error save_err = ResourceSaver::save(scene_save_path, scene);
	memdelete(root_node);
	ERR_FAIL_COND_V_MSG(save_err != OK, save_err, "Cannot save scene to file '" + scene_save_path);

	return OK;
}
