extends Node

const TextToCpp = preload("res://text_to_cpp.gd")


func _ready():
	TextToCpp.export_to_cpp(
		"vrender.glsl",
		"../engine/render_normalmap_shader_template.h", 
		"g_render_normalmap_shader_template",
		TextToCpp.STRUCT_NULL_TERMINATED_ARRAY
	)

	TextToCpp.export_to_cpp(
		"dilate.glsl",
		"../engine/dilate_normalmap_shader.h", 
		"g_dilate_normalmap_shader",
		TextToCpp.STRUCT_NULL_TERMINATED_ARRAY
	)
