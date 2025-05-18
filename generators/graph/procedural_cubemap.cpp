#include "procedural_cubemap.h"
#include "../../constants/voxel_string_names.h"
#include "../../generators/graph/voxel_graph_function.h"
#include "../../util/godot/classes/cubemap.h"
#include "../../util/godot/core/array.h"
#include "../../util/math/conv.h"
#include "../../util/math/vector2f.h"
#include "../../util/profiling.h"

namespace zylann::voxel {

void VoxelProceduralCubemap::set_target_resolution(const unsigned int new_resolution) {
	const unsigned int res = math::clamp<unsigned int>(new_resolution, 4, 4096);
	if (res == _target_resolution) {
		return;
	}
	_target_resolution = res;
	_dirty = true;
}

int VoxelProceduralCubemap::get_target_resolution() const {
	return _target_resolution;
}

void VoxelProceduralCubemap::set_target_format(const Format format) {
	if (format == _target_format) {
		return;
	}
	_target_format = format;
	_dirty = true;
}

VoxelProceduralCubemap::Format VoxelProceduralCubemap::get_target_format() const {
	return _target_format;
}

void VoxelProceduralCubemap::set_graph(Ref<pg::VoxelGraphFunction> graph) {
	if (_graph == graph) {
		return;
	}

	if (_graph.is_valid()) {
		_graph->disconnect(
				VoxelStringNames::get_singleton().changed, callable_mp(this, &VoxelProceduralCubemap::on_graph_changed)
		);
	}

	_graph = graph;

	if (_graph.is_valid()) {
		// Can't compile and update right here... because because Godot loads the resource by setting properties in an
		// unreliable order, so we can only update only when we are sure they were all set. Using call_deferred for this
		// is also not possible because it could be too late.

		_graph->connect(
				VoxelStringNames::get_singleton().changed, callable_mp(this, &VoxelProceduralCubemap::on_graph_changed)
		);
	}

	_dirty = true;
}

Ref<pg::VoxelGraphFunction> VoxelProceduralCubemap::get_graph() const {
	return _graph;
}

void VoxelProceduralCubemap::on_graph_changed() {
	_dirty = true;
}

static Image::Format get_image_format(const VoxelProceduralCubemap::Format src) {
	switch (src) {
		case VoxelProceduralCubemap::FORMAT_L8:
			return Image::FORMAT_L8;
		case VoxelProceduralCubemap::FORMAT_R8:
			return Image::FORMAT_R8;
		case VoxelProceduralCubemap::FORMAT_RF:
			return Image::FORMAT_RF;
		default:
			ZN_PRINT_ERROR("Unhandled format");
			return Image::FORMAT_L8;
	}
}

void VoxelProceduralCubemap::update() {
	ZN_PROFILE_SCOPE();

	_dirty = false;

	if (_graph.is_null()) {
		return;
	}
	if (!_graph->is_compiled()) {
		const pg::CompilationResult result = _graph->compile(false);
		if (!result.success) {
			return;
		}
	}

	{
		std::shared_ptr<pg::VoxelGraphFunction::CompiledGraph> compiled_graph = _graph->get_compiled_graph();
		if (compiled_graph != nullptr) {
			const int input_count = compiled_graph->runtime.get_input_count();
			const int output_count = compiled_graph->runtime.get_output_count();

			if (input_count != 3) {
				return;
			}
			if (output_count != 1) {
				return;
			}
		}
	}

	const Image::Format image_format = get_image_format(_target_format);

	// TODO Handle CPU filtering preservation. Re-pad afterwards? Generate with padding directly?

	if (!is_valid() || get_format() != image_format || get_resolution() != _target_resolution) {
		create(_target_resolution, image_format);
	}

	const int max_chunk_length = 256;

	StdVector<float> x_array;
	StdVector<float> y_array;
	StdVector<float> z_array;

	StdVector<float> r_array;

	x_array.reserve(max_chunk_length);
	y_array.reserve(max_chunk_length);
	z_array.reserve(max_chunk_length);

	r_array.reserve(max_chunk_length);

	for (unsigned int side = 0; side < SIDE_COUNT; ++side) {
		Image &image = get_image(side);

		const Vector2i im_size = image.get_size();
		ZN_ASSERT_RETURN(!Vector2iUtil::is_empty_size(im_size));

		// const Image::Format format = image.get_format();

		const unsigned int im_area = im_size.x * im_size.y;
		const unsigned int last_pixel_index = im_area - 1;

		const Vector2f inv_im_size = Vector2f(1.f) / to_vec2f(im_size);
		unsigned int pixel_start_index = 0;
		unsigned int pixel_index = 0;

		int x0 = 0;
		int y0 = 0;

		for (int y = 0; y < im_size.y; ++y) {
			for (int x = 0; x < im_size.x; ++x, ++pixel_index) {
				// We add 0.5 so we calculate pixel centers
				const Vector2f uv = (Vector2f(x, y) + Vector2f(0.5f)) * inv_im_size;
				const Vector3f cube_pos = get_xyz_from_uv(uv, static_cast<SideIndex>(side));
				const Vector3f sphere_pos = math::normalized(cube_pos);
				x_array.push_back(sphere_pos.x);
				y_array.push_back(sphere_pos.y);
				z_array.push_back(sphere_pos.z);

				if (x_array.size() != max_chunk_length && pixel_index != last_pixel_index) {
					continue;
				}

				r_array.resize(x_array.size());

				std::array<Span<const float>, 3> inputs;
				inputs[0] = to_span(x_array);
				inputs[1] = to_span(y_array);
				inputs[2] = to_span(z_array);

				std::array<Span<float>, 1> outputs;
				outputs[0] = to_span(r_array);

				_graph->execute(to_span(inputs), to_span(outputs));

				// TODO Fast-paths. A bit annoying with Godot's CoW mechanism
				// switch (format) {
				// 	default:
				// Slow generic
				unsigned int i = 0;
				for (int y2 = y0; y2 <= y; ++y2) {
					for (int x2 = x0; x2 <= x; ++x2, ++i) {
						const float v = r_array[i];
						const Color color(v, v, v);
						image.set_pixel(x2, y2, color);
					}
				}
				// }

				x0 = x + 1;
				y0 = y;
				if (x0 == im_size.x) {
					x0 = 0;
					++y0;
				}

				pixel_start_index = pixel_index + 1;

				x_array.clear();
				y_array.clear();
				z_array.clear();
			}
		}

		// image.save_png(String("test_side_{0}.png").format(varray(side)));
	}

	emit_signal(VoxelStringNames::get_singleton().updated);
}

Ref<ZN_Cubemap> VoxelProceduralCubemap::zn_duplicate() const {
	Ref<VoxelProceduralCubemap> d;
	d.instantiate();
	ZN_Cubemap::duplicate_to(**d);
	d->_graph = _graph;
	d->_target_format = _target_format;
	d->_target_resolution = _target_resolution;
	d->_dirty = _dirty;
	return d;
}

void VoxelProceduralCubemap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_graph", "graph"), &VoxelProceduralCubemap::set_graph);
	ClassDB::bind_method(D_METHOD("get_graph"), &VoxelProceduralCubemap::get_graph);

	ClassDB::bind_method(
			D_METHOD("set_target_resolution", "resolution"), &VoxelProceduralCubemap::set_target_resolution
	);
	ClassDB::bind_method(D_METHOD("get_target_resolution"), &VoxelProceduralCubemap::get_target_resolution);

	ClassDB::bind_method(D_METHOD("set_target_format", "format"), &VoxelProceduralCubemap::set_target_format);
	ClassDB::bind_method(D_METHOD("get_target_format"), &VoxelProceduralCubemap::get_target_format);

	ADD_PROPERTY(
			PropertyInfo(
					Variant::OBJECT, "graph", PROPERTY_HINT_RESOURCE_TYPE, pg::VoxelGraphFunction::get_class_static()
			),
			"set_graph",
			"get_graph"
	);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "target_resolution"), "set_target_resolution", "get_target_resolution");

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "target_format", PROPERTY_HINT_ENUM, "R8,L8,RF"),
			"set_target_format",
			"get_target_format"
	);

	ADD_SIGNAL(MethodInfo("updated"));
}

} // namespace zylann::voxel
