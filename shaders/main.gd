extends Node

const TextToCpp = preload("res://text_to_cpp.gd")


func _ready():
	TextToCpp.export_to_cpp(
		"detail_gather_hits.glsl",
		"../engine/detail_gather_hits_shader.h", 
		"g_detail_gather_hits_shader",
		TextToCpp.STRUCT_NULL_TERMINATED_ARRAY
	)

	TextToCpp.export_to_cpp(
		"detail_modifier.glsl",
		"../engine/detail_modifier_shader_template.h", 
		"g_detail_modifier_shader_template",
		TextToCpp.STRUCT_NULL_TERMINATED_ARRAY
	)

	TextToCpp.export_to_cpp(
		"detail_normalmap.glsl",
		"../engine/detail_normalmap_shader.h", 
		"g_detail_normalmap_shader",
		TextToCpp.STRUCT_NULL_TERMINATED_ARRAY
	)

	TextToCpp.export_to_cpp(
		"dilate.glsl",
		"../engine/dilate_normalmap_shader.h", 
		"g_dilate_normalmap_shader",
		TextToCpp.STRUCT_NULL_TERMINATED_ARRAY
	)
