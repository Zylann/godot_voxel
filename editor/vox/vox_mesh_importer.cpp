#include "vox_mesh_importer.h"
#include "../../constants/voxel_string_names.h"
#include "../../meshers/cubes/voxel_mesher_cubes.h"
#include "../../storage/voxel_buffer_internal.h"
#include "../../storage/voxel_memory_pool.h"
#include "../../streams/vox_data.h"
#include "../../util/macros.h"
#include "../../util/profiling.h"
#include "vox_import_funcs.h"

String VoxelVoxMeshImporter::get_importer_name() const {
	return "VoxelVoxMeshImporter";
}

String VoxelVoxMeshImporter::get_visible_name() const {
	return "VoxelVoxMeshImporter";
}

void VoxelVoxMeshImporter::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("vox");
}

String VoxelVoxMeshImporter::get_preset_name(int p_idx) const {
	return "Default";
}

int VoxelVoxMeshImporter::get_preset_count() const {
	return 1;
}

String VoxelVoxMeshImporter::get_save_extension() const {
	return "mesh";
}

String VoxelVoxMeshImporter::get_resource_type() const {
	return "ArrayMesh";
}

float VoxelVoxMeshImporter::get_priority() const {
	// Higher import priority means the importer is preferred over another.
	return 0.0;
}

// int VoxelVoxMeshImporter::get_import_order() const {
// }

void VoxelVoxMeshImporter::get_import_options(List<ImportOption> *r_options, int p_preset) const {
	VoxelStringNames *sn = VoxelStringNames::get_singleton();
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, sn->store_colors_in_texture), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::REAL, sn->scale), 1.f));
	r_options->push_back(ImportOption(
			PropertyInfo(Variant::INT, sn->pivot_mode, PROPERTY_HINT_ENUM, "LowerCorner,SceneOrigin,Center"), 1));
}

bool VoxelVoxMeshImporter::get_option_visibility(const String &p_option,
		const Map<StringName, Variant> &p_options) const {
	return true;
}

struct ForEachModelInstanceArgs {
	const vox::Model *model;
	// Pivot position, which turns out to be at the center in MagicaVoxel
	Vector3i position;
	Basis basis;
};

template <typename F>
static Error for_each_model_instance_in_scene_graph(
		const vox::Data &data, int node_id, Transform transform, int depth, F f) {
	//
	ERR_FAIL_COND_V(depth > 10, ERR_INVALID_DATA);
	const vox::Node *vox_node = data.get_node(node_id);

	switch (vox_node->type) {
		case vox::Node::TYPE_TRANSFORM: {
			const vox::TransformNode *vox_transform_node = reinterpret_cast<const vox::TransformNode *>(vox_node);
			// Calculate global transform of the child
			const Transform child_trans(
					transform.basis * vox_transform_node->rotation.basis,
					transform.xform(vox_transform_node->position.to_vec3()));
			for_each_model_instance_in_scene_graph(data, vox_transform_node->child_node_id, child_trans, depth + 1, f);
		} break;

		case vox::Node::TYPE_GROUP: {
			const vox::GroupNode *vox_group_node = reinterpret_cast<const vox::GroupNode *>(vox_node);
			for (unsigned int i = 0; i < vox_group_node->child_node_ids.size(); ++i) {
				const int child_node_id = vox_group_node->child_node_ids[i];
				for_each_model_instance_in_scene_graph(data, child_node_id, transform, depth + 1, f);
			}
		} break;

		case vox::Node::TYPE_SHAPE: {
			const vox::ShapeNode *vox_shape_node = reinterpret_cast<const vox::ShapeNode *>(vox_node);
			ForEachModelInstanceArgs args;
			args.model = &data.get_model(vox_shape_node->model_id);
			args.position = Vector3i::from_rounded(transform.origin);
			args.basis = transform.basis;
			f(args);
		} break;

		default:
			ERR_FAIL_V(ERR_INVALID_DATA);
			break;
	}

	return OK;
}

template <typename F>
void for_each_model_instance(const vox::Data &vox_data, F f) {
	if (vox_data.get_model_count() == 0) {
		return;
	}
	if (vox_data.get_root_node_id() == -1) {
		// No scene graph
		ForEachModelInstanceArgs args;
		args.model = &vox_data.get_model(0);
		// Put at center to match what MagicaVoxel would do
		args.position = args.model->size / 2;
		args.basis = Basis();
		f(args);
		return;
	}
	for_each_model_instance_in_scene_graph(vox_data, vox_data.get_root_node_id(), Transform(), 0, f);
}

// Find intersecting or touching models, merge their voxels into the same grid, mesh the result, then combine meshes.

struct ModelInstance {
	// Model with baked rotation
	std::unique_ptr<VoxelBufferInternal> voxels;
	// Lowest corner position
	Vector3i position;
};

static void extract_model_instances(const vox::Data &vox_data, std::vector<ModelInstance> &out_instances) {
	// Gather all models and bake their rotations
	for_each_model_instance(vox_data, [&out_instances](ForEachModelInstanceArgs args) {
		ERR_FAIL_COND(args.model == nullptr);
		const vox::Model &model = *args.model;

		Span<const uint8_t> src_color_indices;
		Vector3i dst_size = model.size;

		// Using temporary copy to rotate the data
		std::vector<uint8_t> temp_voxels;

		if (args.basis == Basis()) {
			// No transformation
			src_color_indices = to_span_const(model.color_indexes);
		} else {
			IntBasis basis;
			basis.x = Vector3i::from_cast(args.basis.get_axis(Vector3::AXIS_X));
			basis.y = Vector3i::from_cast(args.basis.get_axis(Vector3::AXIS_Y));
			basis.z = Vector3i::from_cast(args.basis.get_axis(Vector3::AXIS_Z));
			temp_voxels.resize(model.color_indexes.size());
			dst_size = transform_3d_array_zxy(
					to_span_const(model.color_indexes), to_span(temp_voxels), model.size, basis);
			src_color_indices = to_span_const(temp_voxels);
		}

		// TODO Optimization: implement transformation for VoxelBuffers so we can avoid using a temporary copy.
		// Didn't do it yet because VoxelBuffers also have metadata and the `transform_3d_array_zxy` function only works on arrays.
		std::unique_ptr<VoxelBufferInternal> voxels = std::make_unique<VoxelBufferInternal>();
		voxels->create(dst_size);
		voxels->decompress_channel(VoxelBufferInternal::CHANNEL_COLOR);

		Span<uint8_t> dst_color_indices;
		ERR_FAIL_COND(!voxels->get_channel_raw(VoxelBufferInternal::CHANNEL_COLOR, dst_color_indices));

		CRASH_COND(src_color_indices.size() != dst_color_indices.size());
		memcpy(dst_color_indices.data(), src_color_indices.data(), dst_color_indices.size() * sizeof(uint8_t));

		ModelInstance mi;
		mi.voxels = std::move(voxels);
		mi.position = args.position - mi.voxels->get_size() / 2;
		out_instances.push_back(std::move(mi));
	});
}

static bool make_single_voxel_grid(Span<const ModelInstance> instances, Vector3i &out_origin,
		VoxelBufferInternal &out_voxels) {
	// Determine total size
	const ModelInstance &first_instance = instances[0];
	Box3i bounding_box(first_instance.position, first_instance.voxels->get_size());
	for (unsigned int instance_index = 1; instance_index < instances.size(); ++instance_index) {
		const ModelInstance &mi = instances[instance_index];
		bounding_box.merge_with(Box3i(mi.position, mi.voxels->get_size()));
	}

	// Extra sanity check
	// 3 gigabytes
	const size_t limit = 3'000'000'000ull;
	const size_t volume = bounding_box.size.volume();
	ERR_FAIL_COND_V_MSG(volume > limit, false,
			String("Vox data is too big to be meshed as a single mesh ({0}: {0} bytes)")
					.format(varray(bounding_box.size.to_vec3(), SIZE_T_TO_VARIANT(volume))));

	out_voxels.create(bounding_box.size + Vector3i(VoxelMesherCubes::PADDING * 2));
	out_voxels.set_channel_depth(VoxelBufferInternal::CHANNEL_COLOR, VoxelBufferInternal::DEPTH_8_BIT);
	out_voxels.decompress_channel(VoxelBufferInternal::CHANNEL_COLOR);

	for (unsigned int instance_index = 0; instance_index < instances.size(); ++instance_index) {
		const ModelInstance &mi = instances[instance_index];
		ERR_FAIL_COND_V(mi.voxels == nullptr, false);
		out_voxels.copy_from(*mi.voxels, Vector3i(), mi.voxels->get_size(),
				mi.position - bounding_box.pos + Vector3i(VoxelMesherCubes::PADDING),
				VoxelBufferInternal::CHANNEL_COLOR);
	}

	out_origin = bounding_box.pos;
	return true;
}

Error VoxelVoxMeshImporter::import(const String &p_source_file, const String &p_save_path,
		const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files,
		Variant *r_metadata) {
	//
	const bool p_store_colors_in_textures = p_options[VoxelStringNames::get_singleton()->store_colors_in_texture];
	const float p_scale = p_options[VoxelStringNames::get_singleton()->scale];
	const int p_pivot_mode = p_options[VoxelStringNames::get_singleton()->pivot_mode];

	ERR_FAIL_INDEX_V(p_pivot_mode, PIVOT_MODES_COUNT, ERR_INVALID_PARAMETER);

	vox::Data vox_data;
	const Error load_err = vox_data.load_from_file(p_source_file);
	ERR_FAIL_COND_V(load_err != OK, load_err);

	// Get color palette
	Ref<VoxelColorPalette> palette;
	palette.instance();
	for (unsigned int i = 0; i < vox_data.get_palette().size(); ++i) {
		const Color8 color = vox_data.get_palette()[i];
		palette->set_color8(i, color);
	}

	if (vox_data.get_model_count() == 0) {
		// wut
		return ERR_CANT_CREATE;
	}

	Ref<Image> atlas;
	Ref<Mesh> mesh;
	std::vector<unsigned int> surface_index_to_material;
	{
		std::vector<ModelInstance> model_instances;
		extract_model_instances(vox_data, model_instances);

		// From this point we no longer need vox data so we can free some memory
		vox_data.clear();

		// TODO Optimization: this approach uses a lot of memory, might fail on scenes with a large bounding box.
		// One workaround would be to mesh the scene incrementally in chunks, giving up greedy meshing beyond 256 or so.
		Vector3i bounding_box_origin;
		VoxelBufferInternal voxels;
		const bool single_grid_succeeded =
				make_single_voxel_grid(to_span_const(model_instances), bounding_box_origin, voxels);
		ERR_FAIL_COND_V(!single_grid_succeeded, ERR_CANT_CREATE);

		// We no longer need these
		model_instances.clear();

		Ref<VoxelMesherCubes> mesher;
		mesher.instance();
		mesher->set_color_mode(VoxelMesherCubes::COLOR_MESHER_PALETTE);
		mesher->set_palette(palette);
		mesher->set_greedy_meshing_enabled(true);
		mesher->set_store_colors_in_texture(p_store_colors_in_textures);

		Vector3 offset;
		switch (p_pivot_mode) {
			case PIVOT_LOWER_CORNER:
				break;
			case PIVOT_SCENE_ORIGIN:
				offset = bounding_box_origin.to_vec3();
				break;
			case PIVOT_CENTER:
				offset = -((voxels.get_size() - Vector3i(1)) / 2).to_vec3();
				break;
			default:
				ERR_FAIL_V(ERR_BUG);
				break;
		};

		mesh = VoxImportUtils::build_mesh(voxels, **mesher, surface_index_to_material, atlas, p_scale, offset);
		// Deallocate large temporary memory to free space.
		// This is a workaround because VoxelBuffer uses this by default, however it doesn't fit the present use case.
		// Eventually we should avoid using this pool here.
		VoxelMemoryPool::get_singleton()->clear_unused_blocks();
	}

	if (mesh.is_null()) {
		// wut
		return ERR_CANT_CREATE;
	}

	// Save atlas
	// TODO Saving atlases separately is impossible because of https://github.com/godotengine/godot/issues/51163
	// Instead, I do like ResourceImporterScene: I leave them UNCOMPRESSED inside the materials...
	/*String atlas_path;
		if (atlas.is_valid()) {
			atlas_path = String("{0}.atlas{1}.stex").format(varray(p_save_path, model_index));
			const Error save_stex_err = save_stex(atlas, atlas_path, false, 0, true, true, true);
			ERR_FAIL_COND_V_MSG(save_stex_err != OK, save_stex_err,
					String("Failed to save {0}").format(varray(atlas_path)));
		}*/

	// DEBUG
	// if (atlas.is_valid()) {
	// 	atlas->save_png(String("debug_atlas{0}.png").format(varray(model_index)));
	// }

	FixedArray<Ref<SpatialMaterial>, 2> materials;
	for (unsigned int i = 0; i < materials.size(); ++i) {
		Ref<SpatialMaterial> &mat = materials[i];
		mat.instance();
		mat->set_roughness(1.f);
		if (!p_store_colors_in_textures) {
			// In this case we store colors in vertices
			mat->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		}
	}
	materials[1]->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);

	// Assign materials
	if (p_store_colors_in_textures) {
		// Can't share materials at the moment, because each atlas is specific to its mesh
		for (unsigned int surface_index = 0; surface_index < surface_index_to_material.size(); ++surface_index) {
			const unsigned int material_index = surface_index_to_material[surface_index];
			CRASH_COND(material_index >= materials.size());
			Ref<SpatialMaterial> material = materials[material_index]->duplicate();
			if (atlas.is_valid()) {
				// TODO Do I absolutely HAVE to load this texture back to memory AND renderer just so import works??
				//Ref<Texture> texture = ResourceLoader::load(atlas_path);
				// TODO THIS IS A WORKAROUND, it is not supposed to be an ImageTexture...
				// See earlier code, I could not find any way to reference a separate StreamTexture.
				Ref<ImageTexture> texture;
				texture.instance();
				texture->create_from_image(atlas, 0);
				material->set_texture(SpatialMaterial::TEXTURE_ALBEDO, texture);
			}
			mesh->surface_set_material(surface_index, material);
		}
	} else {
		for (unsigned int surface_index = 0; surface_index < surface_index_to_material.size(); ++surface_index) {
			const unsigned int material_index = surface_index_to_material[surface_index];
			CRASH_COND(material_index >= materials.size());
			mesh->surface_set_material(surface_index, materials[material_index]);
		}
	}

	// Save mesh
	{
		VOXEL_PROFILE_SCOPE();
		String mesh_save_path = String("{0}.mesh").format(varray(p_save_path));
		const Error mesh_save_err = ResourceSaver::save(mesh_save_path, mesh);
		ERR_FAIL_COND_V_MSG(mesh_save_err != OK, mesh_save_err,
				String("Failed to save {0}").format(varray(mesh_save_path)));
	}

	return OK;
}
