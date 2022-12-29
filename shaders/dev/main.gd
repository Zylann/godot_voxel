extends Node

const TextToCpp = preload("res://text_to_cpp.gd")


func _ready():
	#_generate_files()
	_export_to_cpp()


func _generate_files():
	# For testing compilation of combined snippets
	
	TextToCpp.generate_file(
		"detail_modifier_template.glsl",
		"modifier_sphere_snippet.glsl",
		"detail_modifier_sphere.gen.glsl"
	)

	TextToCpp.generate_file(
		"detail_modifier_template.glsl",
		"modifier_mesh_snippet.glsl",
		"detail_modifier_mesh.gen.glsl"
	)


func _export_to_cpp():
	var root = "../"
	
	TextToCpp.export_to_cpp(
		"detail_gather_hits.glsl",
		root + "detail_gather_hits_shader.h", 
		"g_detail_gather_hits_shader"
	)

	TextToCpp.export_to_cpp(
		"detail_generator_template.glsl",
		root + "detail_generator_shader_template.h", 
		"g_detail_generator_shader_template"
	)

	TextToCpp.export_to_cpp(
		"detail_modifier_template.glsl",
		root + "detail_modifier_shader_template.h", 
		"g_detail_modifier_shader_template"
	)

	TextToCpp.export_to_cpp(
		"detail_normalmap.glsl",
		root + "detail_normalmap_shader.h", 
		"g_detail_normalmap_shader"
	)

	TextToCpp.export_to_cpp(
		"dilate.glsl",
		root + "dilate_normalmap_shader.h", 
		"g_dilate_normalmap_shader"
	)

	TextToCpp.export_snippet_to_cpp(
		"modifier_sphere_snippet.glsl",
		root + "modifier_sphere_shader_snippet.h", 
		"g_modifier_sphere_shader_snippet"
	)

	TextToCpp.export_snippet_to_cpp(
		"modifier_mesh_snippet.glsl",
		root + "modifier_mesh_shader_snippet.h", 
		"g_modifier_mesh_shader_snippet"
	)

	TextToCpp.export_to_cpp(
		"transvoxel_minimal.gdshader",
		root + "transvoxel_minimal_shader.h", 
		"g_transvoxel_minimal_shader"
	)
