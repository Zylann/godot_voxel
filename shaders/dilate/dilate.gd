extends Node


@onready var _result_texture_rect = $VBoxContainer/Result

var _rendering_device : RenderingDevice
var _shader_rid : RID
var _input_image : Image
var _output_image : Image
var _texture_format : RDTextureFormat
var _params_uniform_buffer_rid : RID


func _ready():
	setup()
	
	_result_texture_rect.texture = ImageTexture.create_from_image(_input_image)
	

func _exit_tree():
	cleanup()


func setup():
	assert(_rendering_device == null)
	
	_rendering_device = RenderingServer.create_local_rendering_device()
	var rd = _rendering_device
	
	# Shader 
	
	var shader_file : RDShaderFile = load("res://dilate.glsl")
	var shader_bytecode := shader_file.get_spirv()
	_shader_rid = rd.shader_create_from_spirv(shader_bytecode)

	# Texture format
	
	_input_image = Image.load_from_file("dilate/dilate_test_input.png")
	_input_image.convert(Image.FORMAT_RGBA8)
	
	var texture_format = RDTextureFormat.new()
	texture_format.width = _input_image.get_width()
	texture_format.height = _input_image.get_height()
	# TODO I could not find RGB8 in GLSL image formats, but we have it in this enum.
	# What does that mean? Is it actually supported? Is it converted?
	texture_format.format = RenderingDevice.DATA_FORMAT_R8G8B8A8_UINT
	texture_format.usage_bits = RenderingDevice.TEXTURE_USAGE_STORAGE_BIT \
		# TODO not sure we need this one, since we only generate the texture
		| RenderingDevice.TEXTURE_USAGE_CAN_UPDATE_BIT \
		| RenderingDevice.TEXTURE_USAGE_CAN_COPY_FROM_BIT
	texture_format.texture_type = RenderingDevice.TEXTURE_TYPE_2D
	_texture_format = texture_format
	
	# Params
	
	# I need only 4 but apparently the minimum size is 16
	_params_uniform_buffer_rid = rd.uniform_buffer_create(16)


func run():
	var rd = _rendering_device
	
	# Source texture
	
	# TODO What is this for?
	var src_texture_view = RDTextureView.new()
	
	var src_texture_rid = rd.texture_create(_texture_format, src_texture_view, 
		[_input_image.get_data()])

	var src_image_uniform = RDUniform.new()
	src_image_uniform.uniform_type = RenderingDevice.UNIFORM_TYPE_IMAGE
	src_image_uniform.binding = 0
	src_image_uniform.add_id(src_texture_rid)

	# Destination texture
	
	var dst_texture_view = RDTextureView.new()
	
	var dst_texture_rid = rd.texture_create(_texture_format, dst_texture_view)

	var dst_image_uniform = RDUniform.new()
	dst_image_uniform.uniform_type = RenderingDevice.UNIFORM_TYPE_IMAGE
	dst_image_uniform.binding = 1
	dst_image_uniform.add_id(dst_texture_rid)
	
	# Params
	
	var tile_size := 16
	var params_pba = PackedByteArray()
	params_pba.resize(4)
	params_pba.encode_s32(0, tile_size)
	rd.buffer_update(_params_uniform_buffer_rid, 0, 4, params_pba)
	var params_uniform = RDUniform.new()
	params_uniform.uniform_type = RenderingDevice.UNIFORM_TYPE_UNIFORM_BUFFER
	params_uniform.binding = 2
	params_uniform.add_id(_params_uniform_buffer_rid)
	
	# Uniform set
	
	var uniform_set_rid = rd.uniform_set_create(
		[src_image_uniform, dst_image_uniform, params_uniform], _shader_rid, 0)
	
	# Pipeline
	
	var pipeline_rid = rd.compute_pipeline_create(_shader_rid)
	
	var local_group_size_x := 8
	var local_group_size_y := 8
	var compute_list_id = rd.compute_list_begin()
	rd.compute_list_bind_compute_pipeline(compute_list_id, pipeline_rid)
	rd.compute_list_bind_uniform_set(compute_list_id, uniform_set_rid, 0)
	rd.compute_list_dispatch(compute_list_id,
		ceildiv(_texture_format.width, local_group_size_x),
		ceildiv(_texture_format.height, local_group_size_y), 1)
	rd.compute_list_end()

	# Work
	
	rd.submit()
	rd.sync()

	# Get result

	var output_bytes = rd.texture_get_data(dst_texture_rid, 0)
	
	_output_image = Image.create_from_data(
		_texture_format.width, _texture_format.height, false, Image.FORMAT_RGBA8, output_bytes)

	#rd.free_rid(uniform_set_rid)
	rd.free_rid(src_texture_rid)
	rd.free_rid(dst_texture_rid)
	rd.free_rid(pipeline_rid)


func cleanup():
	assert(_rendering_device != null)

	_rendering_device.free_rid(_params_uniform_buffer_rid)
	_rendering_device.free_rid(_shader_rid)

	_rendering_device.free()


static func ceildiv(x: int, d: int) -> int:
	assert(d > 0)
	if x > 0:
		return (x + d - 1) / d
	else:
		return x / d


func _on_Run_pressed():
	if _output_image != null:
		_input_image = _output_image
	run()
	_result_texture_rect.texture = ImageTexture.create_from_image(_output_image)
