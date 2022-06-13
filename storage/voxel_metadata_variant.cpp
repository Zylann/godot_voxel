#include "voxel_metadata_variant.h"
#include <core/io/marshalls.h>

namespace zylann::voxel::gd {

size_t VoxelMetadataVariant::get_serialized_size() const {
	int len;
	const Error err = encode_variant(data, nullptr, len, false);
	ERR_FAIL_COND_V_MSG(err != OK, 0, "Error when trying to encode Variant metadata.");
	return len;
}

size_t VoxelMetadataVariant::serialize(Span<uint8_t> dst) const {
	int written_length;
	const Error err = encode_variant(data, dst.data(), written_length, false);
	ERR_FAIL_COND_V(err != OK, 0);
	return written_length;
}

bool VoxelMetadataVariant::deserialize(Span<const uint8_t> src, uint64_t &out_read_size) {
	int read_length;
	const Error err = decode_variant(data, src.data(), src.size(), &read_length, false);
	ERR_FAIL_COND_V_MSG(err != OK, false, "Failed to deserialize block metadata");
	out_read_size = read_length;
	return true;
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
