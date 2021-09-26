#include "vox_importer.h"
#include "../../constants/voxel_string_names.h"
#include "../../meshers/cubes/voxel_mesher_cubes.h"
#include "../../storage/voxel_buffer.h"
#include "../../streams/vox_data.h"
#include "../../util/godot/funcs.h"
#include "../../util/profiling.h"
#include "vox_import_funcs.h"

#include <core/os/file_access.h>
#include <scene/3d/mesh_instance.h>
#include <scene/3d/spatial.h>
#include <scene/resources/mesh.h>
#include <scene/resources/packed_scene.h>

String VoxelVoxImporter::get_importer_name() const {
	return "VoxelVoxImporter";
}

String VoxelVoxImporter::get_visible_name() const {
	return "VoxelVoxSceneImporter";
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

float VoxelVoxImporter::get_priority() const {
	// Higher import priority means the importer is preferred over another.
	// By default, use this importer.
	return 1.0;
}

// int VoxelVoxImporter::get_import_order() const {
// }

void VoxelVoxImporter::get_import_options(List<ImportOption> *r_options, int p_preset) const {
	VoxelStringNames *sn = VoxelStringNames::get_singleton();
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, sn->store_colors_in_texture), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::REAL, sn->scale), 1.f));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, sn->enable_baked_lighting), true));
}

bool VoxelVoxImporter::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {
	return true;
}

static void add_mesh_instance(Ref<Mesh> mesh, Node *parent, Node *owner, Vector3 offset, bool p_enable_baked_lighting) {
	MeshInstance *mesh_instance = memnew(MeshInstance);
	mesh_instance->set_mesh(mesh);
	parent->add_child(mesh_instance);
	mesh_instance->set_owner(owner);
	mesh_instance->set_translation(offset);
	mesh_instance->set_flag(GeometryInstance::FLAG_USE_BAKED_LIGHT, p_enable_baked_lighting);
	// TODO Colliders? Needs conventions or attributes probably.
	// But due to the nature of voxels, users may often prefer to place colliders themselves (slopes notably).
}

struct VoxMesh {
	Ref<Mesh> mesh;
	Vector3 pivot;
};

static Error process_scene_node_recursively(const vox::Data &data, int node_id, Spatial *parent_node,
		Spatial *&root_node, int depth, const Vector<VoxMesh> &meshes, float scale, bool p_enable_baked_lighting) {
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
			node->set_transform(Transform(
					vox_transform_node->rotation.basis,
					vox_transform_node->position.to_vec3() * scale));
			process_scene_node_recursively(
					data, vox_transform_node->child_node_id, node, root_node, depth + 1, meshes, scale,
					p_enable_baked_lighting);

			// If the parent isn't anything special and has only one child,
			// it may be cleaner to flatten the hierarchy. We keep the root node unaffected.
			// TODO Any way to not need a string to check if a node is a specific class?
			if (node != root_node && node->get_class() == "Spatial" && node->get_child_count() == 1) {
				Spatial *child = Object::cast_to<Spatial>(node->get_child(0));
				if (child != nullptr) {
					node->remove_child(child);
					parent_node->remove_child(node);
					child->set_transform(node->get_transform() * child->get_transform());
					// TODO Would be nice if I could just replace the node without any fuss but `replace_by` is too busy
					parent_node->add_child(child);
					// Removal from previous parent unsets the owner, so we have to set it again
					child->set_owner(root_node);
					memdelete(node);
				}
			}
		} break;

		case vox::Node::TYPE_GROUP: {
			const vox::GroupNode *vox_group_node = reinterpret_cast<const vox::GroupNode *>(vox_node);
			for (unsigned int i = 0; i < vox_group_node->child_node_ids.size(); ++i) {
				const int child_node_id = vox_group_node->child_node_ids[i];
				process_scene_node_recursively(
						data, child_node_id, parent_node, root_node, depth + 1, meshes, scale, p_enable_baked_lighting);
			}
		} break;

		case vox::Node::TYPE_SHAPE: {
			ERR_FAIL_COND_V(parent_node == nullptr, ERR_BUG);
			ERR_FAIL_COND_V(root_node == nullptr, ERR_BUG);
			const vox::ShapeNode *vox_shape_node = reinterpret_cast<const vox::ShapeNode *>(vox_node);
			const VoxMesh &mesh_data = meshes[vox_shape_node->model_id];
			ERR_FAIL_COND_V(mesh_data.mesh.is_null(), ERR_BUG);
			const Vector3 offset = -mesh_data.pivot * scale;
			add_mesh_instance(mesh_data.mesh, parent_node, root_node, offset, p_enable_baked_lighting);
		} break;

		default:
			ERR_FAIL_V(ERR_INVALID_DATA);
			break;
	}

	return OK;
}

/*static Error save_stex(const Ref<Image> &p_image, const String &p_to_path,
		bool p_mipmaps, int p_texture_flags, bool p_streamable,
		bool p_detect_3d, bool p_detect_srgb) {
	//
	FileAccess *f = FileAccess::open(p_to_path, FileAccess::WRITE);
	ERR_FAIL_NULL_V(f, ERR_CANT_OPEN);
	f->store_8('G');
	f->store_8('D');
	f->store_8('S');
	f->store_8('T'); //godot streamable texture

	f->store_16(p_image->get_width());
	f->store_16(0);
	f->store_16(p_image->get_height());
	f->store_16(0);

	f->store_32(p_texture_flags);

	uint32_t format = 0;

	if (p_streamable) {
		format |= StreamTexture::FORMAT_BIT_STREAM;
	}
	if (p_mipmaps) {
		format |= StreamTexture::FORMAT_BIT_HAS_MIPMAPS; //mipmaps bit
	}
	if (p_detect_3d) {
		format |= StreamTexture::FORMAT_BIT_DETECT_3D;
	}
	if (p_detect_srgb) {
		format |= StreamTexture::FORMAT_BIT_DETECT_SRGB;
	}

	// COMPRESS_LOSSLESS

	const bool lossless_force_png = ProjectSettings::get_singleton()->get("rendering/lossless_compression/force_png");
	// Note: WebP has a size limit
	const bool use_webp = !lossless_force_png && p_image->get_width() <= 16383 && p_image->get_height() <= 16383;
	Ref<Image> image = p_image->duplicate();
	if (p_mipmaps) {
		image->generate_mipmaps();
	} else {
		image->clear_mipmaps();
	}

	const int mmc = image->get_mipmap_count() + 1;

	if (use_webp) {
		format |= StreamTexture::FORMAT_BIT_WEBP;
	} else {
		format |= StreamTexture::FORMAT_BIT_PNG;
	}
	f->store_32(format);
	f->store_32(mmc);

	for (int i = 0; i < mmc; i++) {
		if (i > 0) {
			image->shrink_x2();
		}

		PoolVector<uint8_t> data;
		if (use_webp) {
			data = Image::webp_lossless_packer(image);
		} else {
			data = Image::png_packer(image);
		}
		const int data_len = data.size();
		f->store_32(data_len);

		PoolVector<uint8_t>::Read r = data.read();
		f->store_buffer(r.ptr(), data_len);
	}

	memdelete(f);
	return OK;
}*/

// template <typename K, typename T>
// static T try_get(const Map<K, T> &map, const K &key, T defval) {
// 	const Map<K, T>::Element *e = map.find(key);
// 	if (e != nullptr) {
// 		return e->value();
// 	}
// 	return defval;
// }

Error VoxelVoxImporter::import(const String &p_source_file, const String &p_save_path,
		const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files,
		Variant *r_metadata) {
	VOXEL_PROFILE_SCOPE();

	const bool p_store_colors_in_textures = p_options[VoxelStringNames::get_singleton()->store_colors_in_texture];
	const float p_scale = p_options[VoxelStringNames::get_singleton()->scale];
	const bool p_enable_baked_lighting = p_options[VoxelStringNames::get_singleton()->enable_baked_lighting];

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
	mesher->set_store_colors_in_texture(p_store_colors_in_textures);

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

	// Build meshes from voxel models
	for (unsigned int model_index = 0; model_index < data.get_model_count(); ++model_index) {
		const vox::Model &model = data.get_model(model_index);

		VoxelBufferInternal voxels;
		voxels.create(model.size + Vector3i(VoxelMesherCubes::PADDING * 2));
		voxels.decompress_channel(VoxelBuffer::CHANNEL_COLOR);

		Span<uint8_t> dst_color_indices;
		ERR_FAIL_COND_V(!voxels.get_channel_raw(VoxelBuffer::CHANNEL_COLOR, dst_color_indices), ERR_BUG);
		Span<const uint8_t> src_color_indices = to_span_const(model.color_indexes);
		copy_3d_region_zxy(dst_color_indices, voxels.get_size(), Vector3i(VoxelMesherCubes::PADDING),
				src_color_indices, model.size, Vector3i(), model.size);

		std::vector<unsigned int> surface_index_to_material;
		Ref<Image> atlas;
		Ref<Mesh> mesh = VoxImportUtils::build_mesh(
				voxels, **mesher, surface_index_to_material, atlas, p_scale, Vector3());

		if (mesh.is_null()) {
			continue;
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

		VoxMesh mesh_info;
		mesh_info.mesh = mesh;
		// In MagicaVoxel scene graph, pivots are at the center of models, not at the lower corner.
		// TODO I don't know if this is correct, but I could not find a reference saying how that pivot should be calculated
		mesh_info.pivot = (voxels.get_size() / 2 - Vector3i(1)).to_vec3();
		meshes.write[model_index] = mesh_info;
	}

	Spatial *root_node = nullptr;
	if (data.get_root_node_id() != -1) {
		// Convert scene graph into a node tree
		process_scene_node_recursively(
				data, data.get_root_node_id(), nullptr, root_node, 0, meshes, p_scale, p_enable_baked_lighting);
		ERR_FAIL_COND_V(root_node == nullptr, ERR_INVALID_DATA);

	} else if (meshes.size() > 0) {
		// Some vox files don't have a scene graph
		root_node = memnew(Spatial);
		const VoxMesh &mesh0 = meshes[0];
		add_mesh_instance(mesh0.mesh, root_node, root_node, Vector3(), p_enable_baked_lighting);
	}

	// Save meshes
	for (int model_index = 0; model_index < meshes.size(); ++model_index) {
		VOXEL_PROFILE_SCOPE();
		Ref<Mesh> mesh = meshes[model_index].mesh;
		String res_save_path = String("{0}.model{1}.mesh").format(varray(p_save_path, model_index));
		// `FLAG_CHANGE_PATH` did not do what I thought it did.
		mesh->set_path(res_save_path);
		const Error mesh_save_err = ResourceSaver::save(res_save_path, mesh);
		ERR_FAIL_COND_V_MSG(mesh_save_err != OK, mesh_save_err,
				String("Failed to save {0}").format(varray(res_save_path)));
	}

	root_node->set_name(p_save_path.get_file().get_basename());

	// Save scene
	{
		VOXEL_PROFILE_SCOPE();
		Ref<PackedScene> scene;
		scene.instance();
		scene->pack(root_node);
		String scene_save_path = p_save_path + ".tscn";
		const Error save_err = ResourceSaver::save(scene_save_path, scene);
		memdelete(root_node);
		ERR_FAIL_COND_V_MSG(save_err != OK, save_err, "Cannot save scene to file '" + scene_save_path);
	}

	return OK;
}
