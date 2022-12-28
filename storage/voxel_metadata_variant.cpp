#include "voxel_metadata_variant.h"

namespace zylann::voxel::gd {

size_t VoxelMetadataVariant::get_serialized_size() const {
	return get_variant_encoded_size(data);
}

size_t VoxelMetadataVariant::serialize(Span<uint8_t> dst) const {
	return encode_variant(data, dst);
}

bool VoxelMetadataVariant::deserialize(Span<const uint8_t> src, uint64_t &out_read_size) {
	size_t read_size = 0;
	const bool success = decode_variant(src, data, read_size);
	out_read_size = read_size;
	return success;
}

ICustomVoxelMetadata *VoxelMetadataVariant::duplicate() {
	VoxelMetadataVariant *d = ZN_NEW(VoxelMetadataVariant);
	d->data = data.duplicate();
	return d;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Variant get_as_variant(const VoxelMetadata &meta) {
	switch (int(meta.get_type())) {
		case METADATA_TYPE_VARIANT: {
			const VoxelMetadataVariant &mv = static_cast<const VoxelMetadataVariant &>(meta.get_custom());
			return mv.data;
		}
		case VoxelMetadata::TYPE_EMPTY: {
			return Variant();
		}
		case VoxelMetadata::TYPE_U64: {
			return Variant(int64_t(meta.get_u64()));
		}
		default:
			ZN_PRINT_ERROR("Unknown VoxelMetadata type");
			return Variant();
	}
}

void set_as_variant(VoxelMetadata &meta, Variant v) {
	if (v.get_type() == Variant::NIL) {
		meta.clear();
	} else {
		if (int(meta.get_type()) == METADATA_TYPE_VARIANT) {
			VoxelMetadataVariant &mv = static_cast<VoxelMetadataVariant &>(meta.get_custom());
			mv.data = v;
			return;
		} else {
			VoxelMetadataVariant *mv = ZN_NEW(VoxelMetadataVariant);
			mv->data = v;
			meta.set_custom(METADATA_TYPE_VARIANT, mv);
		}
	}
}

} // namespace zylann::voxel::gd
