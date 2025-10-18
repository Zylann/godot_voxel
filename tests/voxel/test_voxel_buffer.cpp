#include "test_voxel_buffer.h"
#include "../../edition/voxel_tool_buffer.h"
#include "../../storage/metadata/voxel_metadata_factory.h"
#include "../../storage/metadata/voxel_metadata_variant.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../streams/voxel_block_serializer.h"
#include "../../util/io/log.h"
#include "../../util/string/format.h"
#include "../../util/string/std_string.h"
#include "../../util/string/std_stringstream.h"
#include "../../util/testing/test_macros.h"
#include <array>
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

	ICustomVoxelMetadata *duplicate() override {
		CustomMetadataTest *d = ZN_NEW(CustomMetadataTest);
		*d = *this;
		return d;
	}

	bool operator==(const CustomMetadataTest &other) const {
		return a == other.a && b == other.b && c == other.c;
	}

	uint8_t get_type_index() const override {
		return ID;
	}

	bool equals(const ICustomVoxelMetadata &other) const override {
		if (other.get_type_index() != get_type_index()) {
			return false;
		}
#ifdef DEBUG_ENABLED
		ZN_ASSERT(dynamic_cast<const CustomMetadataTest *>(&other) != nullptr);
#endif
		const CustomMetadataTest &other_self = static_cast<const CustomMetadataTest &>(other);
		return (*this) == other_self;
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
	// Comparison 1
	{
		Ref<godot::VoxelBuffer> vb1;
		vb1.instantiate();
		vb1->create(10, 10, 10);
		vb1->set_voxel_metadata(Vector3i(1, 2, 3), 42);

		Ref<godot::VoxelBuffer> vb2;
		vb2.instantiate();
		vb2->create(10, 10, 10);
		vb2->set_voxel_metadata(Vector3i(1, 2, 3), 42);

		ZN_TEST_ASSERT(vb1->get_buffer().equals(vb2->get_buffer()) == true);
	}
	// Comparison 2
	{
		Ref<godot::VoxelBuffer> vb1;
		vb1.instantiate();
		vb1->create(10, 10, 10);
		vb1->set_voxel_metadata(Vector3i(1, 2, 3), 42);

		Ref<godot::VoxelBuffer> vb2;
		vb2.instantiate();
		vb2->create(10, 10, 10);
		vb2->set_voxel_metadata(Vector3i(5, 6, 7), 42);

		ZN_TEST_ASSERT(vb1->get_buffer().equals(vb2->get_buffer()) == false);
	}
	// Duplication
	{
		Ref<godot::VoxelBuffer> vb1;
		vb1.instantiate();
		vb1->create(10, 10, 10);
		vb1->set_voxel_metadata(Vector3i(1, 2, 3), 42);

		Ref<godot::VoxelBuffer> vb2 = vb1->duplicate(true);

		ZN_TEST_ASSERT(vb1->get_buffer().equals(vb2->get_buffer()));

		vb2->set_voxel_metadata(Vector3i(1, 2, 3), 43);
		ZN_TEST_ASSERT(vb1->get_buffer().equals(vb2->get_buffer()));
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
void load_from_array_litteral_xzy(
		VoxelBuffer &vb,
		const uint8_t channel_index,
		const uint8_t (&array)[N],
		const Vector3i size
) {
	vb.create(size);
	unsigned int i = 0;
	for (int y = 0; y < size.y; ++y) {
		for (int z = 0; z < size.z; ++z) {
			for (int x = 0; x < size.x; ++x) {
				ZN_ASSERT(i < N);
				vb.set_voxel(array[i], Vector3i(x, y, z), channel_index);
				++i;
			}
		}
	}
}

void print_channel_as_ascii(const VoxelBuffer &vb, unsigned int channel, const unsigned int padding) {
	StdStringStream ss;

	Vector3i pos;
	for (pos.y = 0; pos.y < vb.get_size().y; ++pos.y) {
		ss << "Y=" << pos.y << '\n';
		for (pos.z = 0; pos.z < vb.get_size().z; ++pos.z) {
			for (pos.x = 0; pos.x < vb.get_size().x; ++pos.x) {
				const int v = vb.get_voxel(pos, channel);

				{
					int d = 1;
					for (unsigned int i = 0; i < padding; ++i) {
						d *= 10;
						if (v < d) {
							ss << ' ';
						}
					}
				}

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

	// clang-format off
	const uint8_t src_values[] {
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
	// clang-format on

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

	// clang-format off
	const uint8_t dst_values[] {
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
	// clang-format on

	const Vector3i paste_dst_pos(-1, 0, 1);
	const uint8_t src_mask_value = 0;
	const uint8_t dst_mask_value = 3;

	// clang-format off
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
	// clang-format on

	const uint8_t copied_channel_index = VoxelBuffer::CHANNEL_TYPE;
	const uint8_t src_mask_channel_index = VoxelBuffer::CHANNEL_TYPE;
	const uint8_t dst_mask_channel_index = VoxelBuffer::CHANNEL_TYPE;

	load_from_array_litteral_xzy(src, copied_channel_index, src_values, Vector3i(5, 3, 4));
	load_from_array_litteral_xzy(dst, copied_channel_index, dst_values, Vector3i(4, 3, 5));

	paste_src_masked_dst_writable_value(
			to_single_element_span(copied_channel_index),
			src,
			src_mask_channel_index,
			src_mask_value,
			dst,
			paste_dst_pos,
			dst_mask_channel_index,
			dst_mask_value,
			true
	);

	VoxelBuffer expected(VoxelBuffer::ALLOCATOR_DEFAULT);
	expected.create(dst.get_size());
	load_from_array_litteral_xzy(expected, copied_channel_index, expected_values, expected.get_size());

	if (!dst.equals(expected)) {
		print_channel_as_ascii(dst, copied_channel_index, 0);
	}

	ZN_TEST_ASSERT(dst.equals(expected));
}

void test_voxel_buffer_paste_masked_metadata() {
	Ref<zylann::voxel::godot::VoxelBuffer> src_buffer;
	src_buffer.instantiate();
	src_buffer->create(3, 4, 5);

	src_buffer->set_voxel_metadata(Vector3i(1, 1, 1), 100);
	src_buffer->set_voxel_metadata(Vector3i(2, 1, 1), 101);
	src_buffer->set_voxel_metadata(Vector3i(1, 2, 1), 102);
	src_buffer->set_voxel_metadata(Vector3i(2, 2, 1), 103);
	src_buffer->set_voxel_metadata(Vector3i(1, 1, 2), 104);
	src_buffer->set_voxel_metadata(Vector3i(2, 1, 2), 105);
	src_buffer->set_voxel_metadata(Vector3i(1, 2, 2), 106);
	src_buffer->set_voxel_metadata(Vector3i(2, 2, 2), 107);

	// This should not get copied due to masking
	src_buffer->set_voxel_metadata(Vector3i(2, 2, 4), 200);

	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_TYPE;
	src_buffer->set_voxel(1, 0, 0, 0, channel); // Specifically to erase the metadata in dst_buffer
	src_buffer->set_voxel(1, 1, 1, 1, channel);
	src_buffer->set_voxel(1, 2, 1, 1, channel);
	src_buffer->set_voxel(1, 1, 2, 1, channel);
	src_buffer->set_voxel(1, 2, 2, 1, channel);
	src_buffer->set_voxel(1, 1, 1, 2, channel);
	src_buffer->set_voxel(1, 2, 1, 2, channel);
	src_buffer->set_voxel(1, 1, 2, 2, channel);
	src_buffer->set_voxel(1, 2, 2, 2, channel);

	Ref<zylann::voxel::godot::VoxelBuffer> dst_buffer;
	dst_buffer.instantiate();
	dst_buffer->create(8, 8, 8);
	const int dst_default_value = 2;
	dst_buffer->fill(dst_default_value, channel);
	// This metadata will get overwritten, since (0,0,0) has no metadata in src_buffer
	dst_buffer->set_voxel_metadata(Vector3i(1, 2, 3), 300);
	// This one will not get erased because out of range of the pasted area
	const Vector3i preserved_metadata_dst_pos(0, 2, 3);
	dst_buffer->set_voxel_metadata(preserved_metadata_dst_pos, 301);

	Ref<zylann::voxel::godot::VoxelBuffer> dst_buffer_original = dst_buffer->duplicate(true);

	Ref<VoxelTool> vt = dst_buffer->get_voxel_tool();
	const Vector3i dst_paste_origin(1, 2, 3);
	const int mask_value = 0;
	vt->paste_masked(dst_paste_origin, src_buffer, VoxelBuffer::ALL_CHANNELS_MASK, channel, mask_value);

	dst_buffer->get_buffer().check_voxel_metadata_integrity();

	for (int z = 0; z < dst_buffer->get_size().z; ++z) {
		for (int x = 0; x < dst_buffer->get_size().x; ++x) {
			for (int y = 0; y < dst_buffer->get_size().y; ++y) {
				const Vector3i dst_pos(x, y, z);

				const int dst_v = dst_buffer->get_voxel(x, y, z, channel);
				// 0 values must not have been copied
				ZN_TEST_ASSERT(dst_v != 0);

				if (dst_v == dst_default_value) {
					// All cells not pasted onto must have kept their original metadata
					const Variant dst_m = dst_buffer->get_voxel_metadata(dst_pos);
					const Variant dst_m_original = dst_buffer_original->get_voxel_metadata(dst_pos);
					ZN_TEST_ASSERT(dst_m == dst_m_original);
				}
			}
		}
	}

	// print_channel_as_ascii(dst_buffer->get_buffer(), channel);

	for (int z = 0; z < src_buffer->get_size().z; ++z) {
		for (int x = 0; x < src_buffer->get_size().x; ++x) {
			for (int y = 0; y < src_buffer->get_size().y; ++y) {
				const Vector3i src_pos(x, y, z);
				const Vector3i dst_pos = dst_paste_origin + src_pos;

				// Voxel values in the copied area must be equal
				const int src_v = src_buffer->get_voxel(src_pos.x, src_pos.y, src_pos.z, channel);
				const int dst_v = dst_buffer->get_voxel(dst_pos.x, dst_pos.y, dst_pos.z, channel);
				if (src_v == mask_value) {
					ZN_TEST_ASSERT(dst_v == dst_default_value);
				} else {
					ZN_TEST_ASSERT(dst_v == src_v);
				}

				// Metadata in copied area must be equal
				const Variant src_m = src_buffer->get_voxel_metadata(src_pos);
				const Variant dst_m = dst_buffer->get_voxel_metadata(dst_pos);
				if (src_v == mask_value) {
					// Preserved cell
					const Variant dst_m_original = dst_buffer_original->get_voxel_metadata(dst_pos);
					ZN_TEST_ASSERT(dst_m == dst_m_original);
				} else {
					// Overwritten
					ZN_TEST_ASSERT(dst_m == src_m);
				}
			}
		}
	}
}

void test_voxel_buffer_paste_masked_metadata_oob() {
	Ref<zylann::voxel::godot::VoxelBuffer> src_buffer;
	src_buffer.instantiate();
	src_buffer->create(3, 1, 1);

	src_buffer->set_voxel_metadata(Vector3i(0, 0, 0), 100);
	src_buffer->set_voxel_metadata(Vector3i(1, 0, 0), 101);
	src_buffer->set_voxel_metadata(Vector3i(2, 0, 0), 101);

	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_TYPE;
	src_buffer->set_voxel(1, 0, 0, 0, channel);
	src_buffer->set_voxel(1, 1, 0, 0, channel);
	src_buffer->set_voxel(1, 2, 0, 0, channel);

	Ref<zylann::voxel::godot::VoxelBuffer> dst_buffer;
	dst_buffer.instantiate();
	dst_buffer->create(4, 4, 4);
	dst_buffer->fill(2);

	// Paste it out of bounds, only one cell overlaps
	Ref<VoxelTool> vt = dst_buffer->get_voxel_tool();
	vt->paste_masked(Vector3i(3, 2, 2), src_buffer, (1 << VoxelBuffer::CHANNEL_TYPE), channel, 0);

	dst_buffer->get_buffer().check_voxel_metadata_integrity();

	const FlatMapMoveOnly<Vector3i, VoxelMetadata> &vm = dst_buffer->get_buffer().get_voxel_metadata();
	ZN_TEST_ASSERT(vm.size() == 1);

	const Variant dst_m = dst_buffer->get_voxel_metadata(Vector3i(3, 2, 2));
	ZN_TEST_ASSERT(dst_m == Variant(100));

	// for (FlatMapMoveOnly<Vector3i, VoxelMetadata>::ConstIterator it = vm.begin(); it != vm.end(); ++it) {
	// 	ZN_PRINT_VERBOSE(format("Metadata found at {}", it->key));
	// }
}

void test_voxel_buffer_set_channel_bytes() {
	// Set 8-bit non-empty data
	{
		Ref<godot::VoxelBuffer> vb;
		vb.instantiate();
		vb->create(3, 4, 5);
		const godot::VoxelBuffer::ChannelId channel = godot::VoxelBuffer::CHANNEL_TYPE;
		vb->set_channel_depth(channel, godot::VoxelBuffer::DEPTH_8_BIT);

		const uint64_t volume = Vector3iUtil::get_volume_u64(vb->get_size());

		PackedByteArray bytes;
		bytes.resize(volume);
		Span<uint8_t> bytes_s(bytes.ptrw(), bytes.size());
		for (unsigned int i = 0; i < bytes_s.size(); ++i) {
			bytes_s[0] = i;
		}

		vb->set_channel_from_byte_array(channel, bytes);

		{
			Vector3i pos;
			unsigned int i = 0;
			for (pos.z = 0; pos.z < vb->get_size().z; ++pos.z) {
				for (pos.x = 0; pos.x < vb->get_size().x; ++pos.x) {
					for (pos.y = 0; pos.y < vb->get_size().y; ++pos.y) {
						const int v = vb->get_voxel(pos.x, pos.y, pos.z, channel);
						const int expected_v = bytes[i];
						ZN_TEST_ASSERT(v == expected_v);
						++i;
					}
				}
			}
		}
	}
	// Set empty
	// Might error, but should not crash
	{
		Ref<godot::VoxelBuffer> vb;
		vb.instantiate();

		const godot::VoxelBuffer::ChannelId channel = godot::VoxelBuffer::CHANNEL_TYPE;
		vb->set_channel_from_byte_array(channel, PackedByteArray());
	}
}

void test_voxel_buffer_issue769() {
	// indices_to_bitarray was incorrect

	const uint8_t base_values[] = {
		// clang-format off
		0, 1,  2,  3, 
		4, 5,  6,  7, 
		8, 9, 10, 11
		// clang-format on
	};
	const uint8_t pasted_values[] = {
		// clang-format off
		12, 13, 14, 
		15, 16, 17,
		// clang-format on
	};
	const Vector3i pasting_pos(1, 0, 1);
	const std::array<uint8_t, 4> writable_values{ 5, 6, 7, 10 };
	const uint8_t expected_values[] = {
		// clang-format off
		0,  1,  2,  3, 
		4, 12, 13, 14, 
		8,  9, 16, 11
		// clang-format on
	};

	VoxelBuffer base_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
	VoxelBuffer pasted_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
	VoxelBuffer expected_buffer(VoxelBuffer::ALLOCATOR_DEFAULT);

	const uint8_t channel_id = VoxelBuffer::CHANNEL_TYPE;

	load_from_array_litteral_xzy(base_buffer, channel_id, base_values, Vector3i(4, 1, 3));
	load_from_array_litteral_xzy(pasted_buffer, channel_id, pasted_values, Vector3i(3, 1, 2));
	load_from_array_litteral_xzy(expected_buffer, channel_id, expected_values, Vector3i(4, 1, 3));

	DynamicBitset bitarray;
	indices_to_bitarray(to_span(writable_values), bitarray);

	// Check the bitarray
	for (const uint8_t v : writable_values) {
		ZN_TEST_ASSERT(v < bitarray.size());
		ZN_TEST_ASSERT(bitarray.get(v));
	}
	for (uint8_t v = 0; v < bitarray.size(); ++v) {
		if (!contains(to_span(writable_values), v)) {
			ZN_TEST_ASSERT(bitarray.get(v) == false);
		}
	}

	paste_src_masked_dst_writable_bitarray(
			to_single_element_span(channel_id),
			pasted_buffer,
			channel_id,
			99,
			base_buffer,
			pasting_pos,
			channel_id,
			bitarray,
			true
	);

#ifdef DEV_ENABLED
	if (!base_buffer.equals(expected_buffer)) {
		print_line("Result:");
		print_channel_as_ascii(base_buffer, channel_id, 1);
		print_line("Expected:");
		print_channel_as_ascii(expected_buffer, channel_id, 1);
	}
#endif
	ZN_TEST_ASSERT(base_buffer.equals(expected_buffer));
}

} // namespace zylann::voxel::tests
