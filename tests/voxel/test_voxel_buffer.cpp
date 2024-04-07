#include "test_voxel_buffer.h"
#include "../../storage/metadata/voxel_metadata_factory.h"
#include "../../storage/metadata/voxel_metadata_variant.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../streams/voxel_block_serializer.h"
#include "../../util/string/std_stringstream.h"
#include "../testing.h"
#include <sstream>

namespace zylann::voxel::tests {

void test_voxel_buffer_create() {
	// This test was a repro for a memory corruption crash. The point of this test is to check it doesn't crash,
	// so there is no particular conditions to check.
	VoxelBuffer generated_voxels(VoxelBuffer::ALLOCATOR_DEFAULT);
	generated_voxels.create(Vector3i(5, 5, 5));
	generated_voxels.set_voxel_f(-0.7f, 3, 3, 3, VoxelBuffer::CHANNEL_SDF);
	generated_voxels.create(Vector3i(16, 16, 18));
	// This was found to cause memory corruption at this point because channels got re-allocated using the new size,
	// but were filled using the old size, which was greater, and accessed out of bounds memory.
	// The old size was used because the `_size` member was assigned too late in the process.
	// The corruption did not cause a crash here, but somewhere random where malloc was used shortly after.
	generated_voxels.create(Vector3i(1, 16, 18));
}

class CustomMetadataTest : public ICustomVoxelMetadata {
public:
	static const uint8_t ID = VoxelMetadata::TYPE_CUSTOM_BEGIN + 10;

	uint8_t a;
	uint8_t b;
	uint8_t c;

	size_t get_serialized_size() const override {
		// Note, `sizeof(CustomMetadataTest)` gives 16 here. Probably because of vtable
		return 3;
	}

	size_t serialize(Span<uint8_t> dst) const override {
		dst[0] = a;
		dst[1] = b;
		dst[2] = c;
		return get_serialized_size();
	}

	bool deserialize(Span<const uint8_t> src, uint64_t &out_read_size) override {
		a = src[0];
		b = src[1];
		c = src[2];
		out_read_size = get_serialized_size();
		return true;
	}

	virtual ICustomVoxelMetadata *duplicate() {
		CustomMetadataTest *d = ZN_NEW(CustomMetadataTest);
		*d = *this;
		return d;
	}

	bool operator==(const CustomMetadataTest &other) const {
		return a == other.a && b == other.b && c == other.c;
	}
};

void test_voxel_buffer_metadata() {
	// Basic get and set
	{
		VoxelBuffer vb(VoxelBuffer::ALLOCATOR_DEFAULT);
		vb.create(10, 10, 10);

		VoxelMetadata *meta = vb.get_or_create_voxel_metadata(Vector3i(1, 2, 3));
		ZN_TEST_ASSERT(meta != nullptr);
		meta->set_u64(1234567890);

		const VoxelMetadata *meta2 = vb.get_voxel_metadata(Vector3i(1, 2, 3));
		ZN_TEST_ASSERT(meta2 != nullptr);
		ZN_TEST_ASSERT(meta2->get_type() == meta->get_type());
		ZN_TEST_ASSERT(meta2->get_u64() == meta->get_u64());
	}
	// Serialization
	{
		VoxelBuffer vb(VoxelBuffer::ALLOCATOR_DEFAULT);
		vb.create(10, 10, 10);

		{
			VoxelMetadata *meta0 = vb.get_or_create_voxel_metadata(Vector3i(1, 2, 3));
			ZN_TEST_ASSERT(meta0 != nullptr);
			meta0->set_u64(1234567890);
		}

		{
			VoxelMetadata *meta1 = vb.get_or_create_voxel_metadata(Vector3i(4, 5, 6));
			ZN_TEST_ASSERT(meta1 != nullptr);
			meta1->clear();
		}

		struct RemoveTypeOnExit {
			~RemoveTypeOnExit() {
				VoxelMetadataFactory::get_singleton().remove_constructor(CustomMetadataTest::ID);
			}
		};
		RemoveTypeOnExit rmtype;
		VoxelMetadataFactory::get_singleton().add_constructor_by_type<CustomMetadataTest>(CustomMetadataTest::ID);
		{
			VoxelMetadata *meta2 = vb.get_or_create_voxel_metadata(Vector3i(7, 8, 9));
			ZN_TEST_ASSERT(meta2 != nullptr);
			CustomMetadataTest *custom = ZN_NEW(CustomMetadataTest);
			custom->a = 10;
			custom->b = 20;
			custom->c = 30;
			meta2->set_custom(CustomMetadataTest::ID, custom);
		}

		BlockSerializer::SerializeResult sresult = BlockSerializer::serialize(vb);
		ZN_TEST_ASSERT(sresult.success);
		StdVector<uint8_t> bytes = sresult.data;

		VoxelBuffer rvb(VoxelBuffer::ALLOCATOR_DEFAULT);
		ZN_TEST_ASSERT(BlockSerializer::deserialize(to_span(bytes), rvb));

		const FlatMapMoveOnly<Vector3i, VoxelMetadata> &vb_meta_map = vb.get_voxel_metadata();
		const FlatMapMoveOnly<Vector3i, VoxelMetadata> &rvb_meta_map = rvb.get_voxel_metadata();

		ZN_TEST_ASSERT(vb_meta_map.size() == rvb_meta_map.size());

		for (auto it = vb_meta_map.begin(); it != vb_meta_map.end(); ++it) {
			const VoxelMetadata &meta = it->value;
			const VoxelMetadata *rmeta = rvb_meta_map.find(it->key);

			ZN_TEST_ASSERT(rmeta != nullptr);
			ZN_TEST_ASSERT(rmeta->get_type() == meta.get_type());

			switch (meta.get_type()) {
				case VoxelMetadata::TYPE_EMPTY:
					break;
				case VoxelMetadata::TYPE_U64:
					ZN_TEST_ASSERT(meta.get_u64() == rmeta->get_u64());
					break;
				case CustomMetadataTest::ID: {
					const CustomMetadataTest &custom = static_cast<const CustomMetadataTest &>(meta.get_custom());
					const CustomMetadataTest &rcustom = static_cast<const CustomMetadataTest &>(rmeta->get_custom());
					ZN_TEST_ASSERT(custom == rcustom);
				} break;
				default:
					ZN_TEST_ASSERT(false);
					break;
			}
		}
	}
}

void test_voxel_buffer_metadata_gd() {
	// Basic get and set (Godot)
	{
		Ref<godot::VoxelBuffer> vb;
		vb.instantiate();
		vb->create(10, 10, 10);

		Array meta;
		meta.push_back("Hello");
		meta.push_back("World");
		meta.push_back(42);

		vb->set_voxel_metadata(Vector3i(1, 2, 3), meta);

		Array read_meta = vb->get_voxel_metadata(Vector3i(1, 2, 3));
		ZN_TEST_ASSERT(read_meta.size() == meta.size());
		ZN_TEST_ASSERT(read_meta == meta);
	}
	// Serialization (Godot)
	{
		Ref<godot::VoxelBuffer> vb;
		vb.instantiate();
		vb->create(10, 10, 10);

		{
			Array meta0;
			meta0.push_back("Hello");
			meta0.push_back("World");
			meta0.push_back(42);
			vb->set_voxel_metadata(Vector3i(1, 2, 3), meta0);
		}
		{
			Dictionary meta1;
			meta1["One"] = 1;
			meta1["Two"] = 2.5;
			meta1["Three"] = Basis();
			vb->set_voxel_metadata(Vector3i(4, 5, 6), meta1);
		}

		BlockSerializer::SerializeResult sresult = BlockSerializer::serialize(vb->get_buffer());
		ZN_TEST_ASSERT(sresult.success);
		StdVector<uint8_t> bytes = sresult.data;

		Ref<godot::VoxelBuffer> vb2;
		vb2.instantiate();

		ZN_TEST_ASSERT(BlockSerializer::deserialize(to_span(bytes), vb2->get_buffer()));

		ZN_TEST_ASSERT(vb2->get_buffer().equals(vb->get_buffer()));

		// `equals` does not compare metadata at the moment, mainly because it's not trivial and there is no use case
		// for it apart from this test, so do it manually

		const FlatMapMoveOnly<Vector3i, VoxelMetadata> &vb_meta_map = vb->get_buffer().get_voxel_metadata();
		const FlatMapMoveOnly<Vector3i, VoxelMetadata> &vb2_meta_map = vb2->get_buffer().get_voxel_metadata();

		ZN_TEST_ASSERT(vb_meta_map.size() == vb2_meta_map.size());

		for (auto it = vb_meta_map.begin(); it != vb_meta_map.end(); ++it) {
			const VoxelMetadata &meta = it->value;
			ZN_TEST_ASSERT(meta.get_type() == godot::METADATA_TYPE_VARIANT);

			const VoxelMetadata *meta2 = vb2_meta_map.find(it->key);
			ZN_TEST_ASSERT(meta2 != nullptr);
			ZN_TEST_ASSERT(meta2->get_type() == meta.get_type());

			const godot::VoxelMetadataVariant &metav =
					static_cast<const godot::VoxelMetadataVariant &>(meta.get_custom());
			const godot::VoxelMetadataVariant &meta2v =
					static_cast<const godot::VoxelMetadataVariant &>(meta2->get_custom());
			ZN_TEST_ASSERT(metav.data == meta2v.data);
		}
	}
}

namespace {

template <size_t N>
void load_from_array_literal(VoxelBuffer &vb, unsigned int channel, const uint8_t (&array)[N], Vector3i size) {
	vb.create(size);
	unsigned int i = 0;
	for (int y = 0; y < size.y; ++y) {
		for (int z = 0; z < size.z; ++z) {
			for (int x = 0; x < size.x; ++x) {
				ZN_ASSERT(i < N);
				vb.set_voxel(array[i], Vector3i(x, y, z), channel);
				++i;
			}
		}
	}
}

void print_channel_as_ascii(const VoxelBuffer &vb, unsigned int channel) {
	StdStringStream ss;

	Vector3i pos;
	for (pos.y = 0; pos.y < vb.get_size().y; ++pos.y) {
		ss << "Y=" << pos.y << '\n';
		for (pos.z = 0; pos.z < vb.get_size().z; ++pos.z) {
			for (pos.x = 0; pos.x < vb.get_size().x; ++pos.x) {
				const int v = vb.get_voxel(pos, channel);
				ss << v << ' ';
			}
			ss << '\n';
		}
	}

	print_line(ss.str());
}

} // namespace

void test_voxel_buffer_paste_masked() {
	VoxelBuffer src(VoxelBuffer::ALLOCATOR_DEFAULT);
	VoxelBuffer dst(VoxelBuffer::ALLOCATOR_DEFAULT);

	const uint8_t src_values[] = {
		0, 0, 0, 0, 0, //
		0, 1, 0, 0, 0, //
		0, 1, 1, 0, 0, //
		0, 1, 1, 1, 0, //
		//
		0, 0, 1, 1, 0, //
		0, 0, 1, 1, 0, //
		0, 0, 1, 1, 0, //
		0, 0, 1, 1, 0, //
		//
		0, 1, 1, 1, 0, //
		0, 0, 0, 0, 0, //
		0, 0, 0, 0, 0, //
		0, 1, 1, 1, 0, //
	};

	// const uint8_t src_values_pretransformed[] = {
	// 	0, 0, 0, 0, //
	// 	0, 0, 0, 0, //
	// 	1, 0, 0, 0, //
	// 	1, 1, 0, 0, //
	// 	1, 1, 1, 0, //
	// 	//
	// 	0, 0, 0, 0, //
	// 	0, 1, 1, 0, //
	// 	0, 1, 1, 0, //
	// 	0, 1, 1, 0, //
	// 	0, 1, 1, 0, //
	// 	//
	// 	0, 0, 0, 0, //
	// 	1, 1, 1, 0, //
	// 	0, 0, 0, 0, //
	// 	0, 0, 0, 0, //
	// 	1, 1, 1, 0, //
	// };

	const uint8_t dst_values[] = {
		3, 3, 3, 3, //
		3, 3, 3, 3, //
		3, 3, 3, 3, //
		3, 3, 3, 3, //
		3, 3, 3, 3, //
		//
		2, 2, 2, 2, //
		2, 2, 2, 2, //
		3, 3, 3, 3, //
		3, 3, 3, 3, //
		3, 3, 3, 3, //
		//
		3, 3, 3, 3, //
		3, 2, 2, 3, //
		3, 2, 2, 3, //
		3, 3, 3, 3, //
		3, 3, 3, 3, //
		//
	};

	const Vector3i paste_dst_pos(-1, 0, 1);
	const uint8_t src_mask_value = 0;
	const uint8_t dst_mask_value = 3;

	const uint8_t expected_values[] = {
		3, 3, 3, 3, //
		3, 3, 3, 3, //
		1, 3, 3, 3, //
		1, 1, 3, 3, //
		1, 1, 1, 3, //
		//
		2, 2, 2, 2, //
		2, 2, 2, 2, //
		3, 1, 1, 3, //
		3, 1, 1, 3, //
		3, 1, 1, 3, //
		//
		3, 3, 3, 3, //
		1, 2, 2, 3, //
		3, 2, 2, 3, //
		3, 3, 3, 3, //
		1, 1, 1, 3, //
		//
	};

	const uint8_t copied_channel_index = 0;
	const uint8_t src_mask_channel_index = 0;
	const uint8_t dst_mask_channel_index = 0;

	load_from_array_literal(src, copied_channel_index, src_values, Vector3i(5, 3, 4));
	load_from_array_literal(dst, copied_channel_index, dst_values, Vector3i(4, 3, 5));

	paste_src_masked_dst_writable_value( //
			to_single_element_span(copied_channel_index), //
			src, //
			src_mask_channel_index, //
			src_mask_value, //
			dst, //
			paste_dst_pos, //
			dst_mask_channel_index, //
			dst_mask_value, //
			true //
	);

	VoxelBuffer expected(VoxelBuffer::ALLOCATOR_DEFAULT);
	expected.create(dst.get_size());
	load_from_array_literal(expected, copied_channel_index, expected_values, expected.get_size());

	if (!dst.equals(expected)) {
		print_channel_as_ascii(dst, copied_channel_index);
	}

	ZN_TEST_ASSERT(dst.equals(expected));
}

} // namespace zylann::voxel::tests
