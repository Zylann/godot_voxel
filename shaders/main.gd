extends Node

const TextToCpp = preload("res://text_to_cpp.gd")


func _ready():
	TextToCpp.export_to_cpp("vrender.glsl", "../engine/render_normalmap_shader_template.h", 
		"g_render_normalmap_shader_template", TextToCpp.STRUCT_NULL_TERMINATED_ARRAY)

