#include "test_voxel_instancer.h"
#include "../../generators/voxel_generator.h"
#include "../../streams/instance_data.h"
#include "../../terrain/instancing/voxel_instance_generator.h"
#include "../../util/godot/classes/array_mesh.h"
#include "../../util/godot/core/packed_arrays.h"
#include "../../util/math/conv.h"
#include "../../util/testing/test_macros.h"

namespace zylann::voxel::tests {

void test_instance_data_serialization() {
	struct L {
		static InstanceBlockData::InstanceData create_instance(
				const float x,
				const float y,
				const float z,
				const float rotx,
				const float roty,
				const float rotz,
				const float scale
		) {
			InstanceBlockData::InstanceData d;
			d.transform = to_transform3f(Transform3D(
					Basis().rotated(Vector3(rotx, roty, rotz)).scaled(Vector3(scale, scale, scale)), Vector3(x, y, z)
			));
			return d;
		}
	};

	// Create some example data
	InstanceBlockData src_data;
	{
		src_data.position_range = 30;
		{
			InstanceBlockData::LayerData layer;
			layer.id = 1;
			layer.scale_min = 1.f;
			layer.scale_max = 1.f;
			layer.instances.push_back(L::create_instance(0, 0, 0, 0, 0, 0, 1));
			layer.instances.push_back(L::create_instance(10, 0, 0, 3.14, 0, 0, 1));
			layer.instances.push_back(L::create_instance(0, 20, 0, 0, 3.14, 0, 1));
			layer.instances.push_back(L::create_instance(0, 0, 30, 0, 0, 3.14, 1));
			src_data.layers.push_back(layer);
		}
		{
			InstanceBlockData::LayerData layer;
			layer.id = 2;
			layer.scale_min = 1.f;
			layer.scale_max = 4.f;
			layer.instances.push_back(L::create_instance(0, 1, 0, 0, 0, 0, 1));
			layer.instances.push_back(L::create_instance(20, 1, 0, -2.14, 0, 0, 2));
			layer.instances.push_back(L::create_instance(0, 20, 0, 0, -2.14, 0, 3));
			layer.instances.push_back(L::create_instance(0, 1, 20, -1, 0, 2.14, 4));
			src_data.layers.push_back(layer);
		}
	}

	StdVector<uint8_t> serialized_data;

	ZN_TEST_ASSERT(serialize_instance_block_data(src_data, serialized_data));

	InstanceBlockData dst_data;
	ZN_TEST_ASSERT(deserialize_instance_block_data(dst_data, to_span_const(serialized_data)));

	// Compare blocks
	ZN_TEST_ASSERT(src_data.layers.size() == dst_data.layers.size());
	ZN_TEST_ASSERT(dst_data.position_range >= 0.f);
	ZN_TEST_ASSERT(dst_data.position_range == src_data.position_range);

	const float distance_error = math::max(src_data.position_range, InstanceBlockData::POSITION_RANGE_MINIMUM) /
			float(InstanceBlockData::POSITION_RESOLUTION);

	// Compare layers
	for (unsigned int layer_index = 0; layer_index < dst_data.layers.size(); ++layer_index) {
		const InstanceBlockData::LayerData &src_layer = src_data.layers[layer_index];
		const InstanceBlockData::LayerData &dst_layer = dst_data.layers[layer_index];

		ZN_TEST_ASSERT(src_layer.id == dst_layer.id);
		if (src_layer.scale_max - src_layer.scale_min < InstanceBlockData::SIMPLE_11B_V1_SCALE_RANGE_MINIMUM) {
			ZN_TEST_ASSERT(src_layer.scale_min == dst_layer.scale_min);
		} else {
			ZN_TEST_ASSERT(src_layer.scale_min == dst_layer.scale_min);
			ZN_TEST_ASSERT(src_layer.scale_max == dst_layer.scale_max);
		}
		ZN_TEST_ASSERT(src_layer.instances.size() == dst_layer.instances.size());

		const float scale_error =
				math::max(
						src_layer.scale_max - src_layer.scale_min, InstanceBlockData::SIMPLE_11B_V1_SCALE_RANGE_MINIMUM
				) /
				float(InstanceBlockData::SIMPLE_11B_V1_SCALE_RESOLUTION);

		const float rotation_error = 2.f / float(InstanceBlockData::SIMPLE_11B_V1_QUAT_RESOLUTION);

		// Compare instances
		for (unsigned int instance_index = 0; instance_index < src_layer.instances.size(); ++instance_index) {
			const InstanceBlockData::InstanceData &src_instance = src_layer.instances[instance_index];
			const InstanceBlockData::InstanceData &dst_instance = dst_layer.instances[instance_index];

			ZN_TEST_ASSERT(
					math::distance(src_instance.transform.origin, dst_instance.transform.origin) <= distance_error
			);

			const Basis src_basis = to_basis3(src_instance.transform.basis);
			const Basis dst_basis = to_basis3(dst_instance.transform.basis);

			const Vector3 src_scale = src_basis.get_scale();
			const Vector3 dst_scale = src_basis.get_scale();
			ZN_TEST_ASSERT(src_scale.distance_to(dst_scale) <= scale_error);

			// Had to normalize here because Godot doesn't want to give you a Quat if the basis is scaled (even
			// uniformly)
			const Quaternion src_rot = src_basis.orthonormalized().get_quaternion();
			const Quaternion dst_rot = dst_basis.orthonormalized().get_quaternion();
			const float rot_dx = Math::abs(src_rot.x - dst_rot.x);
			const float rot_dy = Math::abs(src_rot.y - dst_rot.y);
			const float rot_dz = Math::abs(src_rot.z - dst_rot.z);
			const float rot_dw = Math::abs(src_rot.w - dst_rot.w);
			ZN_TEST_ASSERT(rot_dx <= rotation_error);
			ZN_TEST_ASSERT(rot_dy <= rotation_error);
			ZN_TEST_ASSERT(rot_dz <= rotation_error);
			ZN_TEST_ASSERT(rot_dw <= rotation_error);
		}
	}
}

void test_instance_generator_material_filter_issue774() {
	struct Attrib {
		uint32_t packed_indices;
		uint32_t packed_weights;

		static uint32_t unorm_to_u8(float v) {
			return static_cast<uint32_t>(v * 255.f);
		}

		static Attrib make(
				const uint32_t i0,
				const uint32_t i1,
				const uint32_t i2,
				const uint32_t i3,
				const float w0,
				const float w1,
				const float w2,
				const float w3
		) {
			Attrib a;
			a.packed_indices = i0 | (i1 << 8) | (i2 << 16) | (i3 << 24);
			const uint32_t wi0 = unorm_to_u8(w0);
			const uint32_t wi1 = unorm_to_u8(w1);
			const uint32_t wi2 = unorm_to_u8(w2);
			const uint32_t wi3 = unorm_to_u8(w3);
			a.packed_weights = wi0 | (wi1 << 8) | (wi2 << 16) | (wi3 << 24);
			return a;
		}
	};

	Array mesh_arrays;
	{
		//    o
		//   /|   Z
		//  / |   |
		// o--o   o--X

		PackedVector3Array vertices;
		vertices.push_back(Vector3(0, 0, 0));
		vertices.push_back(Vector3(1, 0, 0));
		vertices.push_back(Vector3(1, 0, 1));

		PackedVector3Array normals;
		normals.push_back(Vector3(0, 1, 0));
		normals.push_back(Vector3(0, 1, 0));
		normals.push_back(Vector3(0, 1, 1));

		PackedInt32Array indices;
		indices.push_back(0);
		indices.push_back(1);
		indices.push_back(2);

		StdVector<Attrib> attribs;
		attribs.push_back(Attrib::make(0, 1, 2, 3, 1.f, 0.f, 0.f, 0.f));
		attribs.push_back(Attrib::make(0, 1, 2, 3, 0.f, 1.f, 0.f, 0.f));
		attribs.push_back(Attrib::make(0, 1, 2, 3, 0.f, 1.f, 0.f, 0.f));
		PackedFloat32Array custom1;
		custom1.resize(2 * attribs.size());
		Span<const float> attribs_f = to_span(attribs).reinterpret_cast_to<const float>();
		zylann::godot::copy_to(custom1, attribs_f);

		mesh_arrays.resize(ArrayMesh::ARRAY_MAX);
		mesh_arrays[ArrayMesh::ARRAY_VERTEX] = vertices;
		mesh_arrays[ArrayMesh::ARRAY_NORMAL] = normals;
		mesh_arrays[ArrayMesh::ARRAY_CUSTOM1] = custom1;
		mesh_arrays[ArrayMesh::ARRAY_INDEX] = indices;
	}

	StdVector<Transform3f> transforms;

	Ref<VoxelInstanceGenerator> generator;
	generator.instantiate();
	generator->set_emit_mode(VoxelInstanceGenerator::EMIT_FROM_FACES);
	generator->set_density(5.f);
	generator->set_voxel_material_filter_enabled(true);
	generator->set_voxel_material_filter_mask(1 << 0);
	generator->set_voxel_material_filter_threshold(0.f);
	generator->generate_transforms(
			transforms,
			Vector3i(0, 0, 0),
			0,
			0,
			mesh_arrays,
			-1,
			-1,
			UP_MODE_POSITIVE_Y,
			0xff,
			16.f,
			Ref<VoxelGenerator>()
	);

	ZN_TEST_ASSERT(transforms.size() > 0);
}

} // namespace zylann::voxel::tests
