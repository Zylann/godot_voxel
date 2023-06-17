#ifndef VOXEL_METADATA_H
#define VOXEL_METADATA_H

#include "../util/memory.h"
//#include "../util/non_copyable.h"
#include "../util/span.h"
#include <cstdint>

namespace zylann::voxel {

// Voxel metadata is arbitrary, sparse data that can be attached to particular voxels.
// It is not intended at being an efficient or fast storage method, but rather a versatile one for special cases.
// For example, it can be used to store text, tags, inventory contents, or other complex states attached to a voxel.
// If you need to store smaller data much more frequently, you may rely on a data channel instead.

// Base interface for custom data types.
class ICustomVoxelMetadata {
public:
	virtual ~ICustomVoxelMetadata() {}

	// Gets how many bytes this metadata will occupy when serialized.
	virtual size_t get_serialized_size() const = 0;

	// Serializes this metadata into `dst`. The size of `dst` will be equal or greater than the size returned by
	// `get_serialized_size()`. Returns how many bytes were written.
	virtual size_t serialize(Span<uint8_t> dst) const = 0;

	// Deserializes this metadata from the given bytes.
	// Returns `true` on success, `false` otherwise. `out_read_size` must be assigned to the number of bytes read.
	virtual bool deserialize(Span<const uint8_t> src, uint64_t &out_read_size) = 0;

	virtual ICustomVoxelMetadata *duplicate() = 0;
};

// Container for one metadata instance. It owns the data.
class VoxelMetadata {
public:
	enum Type : uint8_t { //
		TYPE_EMPTY = 0,
		TYPE_U64 = 1,
		// Reserved predefined types.

		TYPE_CUSTOM_BEGIN = 32,
		// Types equal or greater will implement `ICustomVoxelMetadata`.

		TYPE_APP_SPECIFIC_BEGIN = 40
		// Nothing prevents registering custom types lower than this index, but for convenience, it should be used for
		// application-specific types (aka game-specific). Lower indices can be used for engine-specific integrations.
	};

	static const unsigned int CUSTOM_TYPES_MAX_COUNT = 256 - TYPE_CUSTOM_BEGIN;

	VoxelMetadata() {}

	VoxelMetadata(VoxelMetadata &&other) {
		_type = other._type;
		_data = other._data;
		other._type = TYPE_EMPTY;
	}

	~VoxelMetadata() {
		clear();
	}

	inline void operator=(VoxelMetadata &&other) {
		clear();
		_type = other._type;
		_data = other._data;
		other._type = TYPE_EMPTY;
	}

	void clear();

	inline Type get_type() const {
		return Type(_type);
	}

	void set_u64(const uint64_t &v);
	uint64_t get_u64() const;

	void set_custom(uint8_t type, ICustomVoxelMetadata *custom_data);
	ICustomVoxelMetadata &get_custom();
	const ICustomVoxelMetadata &get_custom() const;

	// Clears this metadata and makes it a duplicate of the given one.
	void copy_from(const VoxelMetadata &src);

private:
	union Data {
		uint64_t u64_data;
		ICustomVoxelMetadata *custom_data;
	};

	uint8_t _type = TYPE_EMPTY;
	Data _data;
};

// Registry of custom metadata types, used to deserialize them from saved data.
class VoxelMetadataFactory {
public:
	typedef ICustomVoxelMetadata *(*ConstructorFunc)();

	static VoxelMetadataFactory &get_singleton();

	VoxelMetadataFactory();

	// Registers a custom metadata type.
	// The `type` you choose should remain the same over time.
	// It will be used in save files, so changing it could break old saves.
	// `type` must be greater or equal to `VoxelMetadata::TYPE_CUSTOM_BEGIN`.
	void add_constructor(uint8_t type, ConstructorFunc ctor);

	template <typename T>
	void add_constructor_by_type(uint8_t type) {
		add_constructor(type, []() { //
			// Doesn't compile if I directly return the newed instance
			ICustomVoxelMetadata *c = ZN_NEW(T);
			return c;
		});
	}

	void remove_constructor(uint8_t type);

	// Constructs a custom metadata type from the given type ID.
	// The `type` must be greater or equal to `VoxelMetadata::TYPE_CUSTOM_BEGIN`.
	// Returns `nullptr` if the type could not be constructed.
	ICustomVoxelMetadata *try_construct(uint8_t type) const;

private:
	FixedArray<ConstructorFunc, VoxelMetadata::CUSTOM_TYPES_MAX_COUNT> _constructors;
};

} //namespace zylann::voxel

#endif // VOXEL_METADATA_H
