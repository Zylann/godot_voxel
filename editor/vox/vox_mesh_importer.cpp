#include "vox_mesh_importer.h"
#include "../../constants/voxel_string_names.h"
#include "../../meshers/cubes/voxel_mesher_cubes.h"
#include "../../storage/voxel_buffer.h"
#include "../../streams/vox_data.h"
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
}

bool VoxelVoxMeshImporter::get_option_visibility(const String &p_option,
		const Map<StringName, Variant> &p_options) const {
	return true;
}

Error VoxelVoxMeshImporter::import(const String &p_source_file, const String &p_save_path,
		const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files,
		Variant *r_metadata) {
	//
	const bool p_store_colors_in_textures = p_options[VoxelStringNames::get_singleton()->store_colors_in_texture];
	const float p_scale = p_options[VoxelStringNames::get_singleton()->scale];

	vox::Data data;
	const Error load_err = data.load_from_file(p_source_file);
	ERR_FAIL_COND_V(load_err != OK, load_err);

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

	if (data.get_model_count() == 0) {
		// wut
		return ERR_CANT_CREATE;
	}
	// TODO Merge all models into one
	const vox::Model &model = data.get_model(0);

	Ref<VoxelBuffer> voxels;
	voxels.instance();
	voxels->create(model.size + Vector3i(VoxelMesherCubes::PADDING * 2));
	voxels->decompress_channel(VoxelBuffer::CHANNEL_COLOR);

	Span<uint8_t> dst_color_indices;
	ERR_FAIL_COND_V(!voxels->get_channel_raw(VoxelBuffer::CHANNEL_COLOR, dst_color_indices), ERR_BUG);
	Span<const uint8_t> src_color_indices = to_span_const(model.color_indexes);
	copy_3d_region_zxy(dst_color_indices, voxels->get_size(), Vector3i(VoxelMesherCubes::PADDING),
			src_color_indices, model.size, Vector3i(), model.size);

	std::vector<unsigned int> surface_index_to_material;
	Ref<Image> atlas;
	Ref<Mesh> mesh = VoxImportUtils::build_mesh(**voxels, **mesher, surface_index_to_material, atlas, p_scale);

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
