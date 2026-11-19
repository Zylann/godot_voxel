#ifndef BLOCKY_ROUND_GEOMETRY_TABLE_H
#define BLOCKY_ROUND_GEOMETRY_TABLE_H

#include "blocky_round_geometry_data.h"
#include <cstddef>

namespace zylann::voxel::blocky::round_geometry {

// Material-independent shape keys reachable by shape_key's one-voxel neighborhood.
// Zero uses the original cube mesher and does not need a generated template.
StdVector<uint64_t> generate_reachable_shape_keys();

// Owned by BakedLibrary. Only bake() may update this under its write lock.
// Meshing readers share the completed table under the library's read lock; get()
// never allocates, generates geometry, or changes the table.
// Local triangle indices retain the generator's order. Vertex IDs address the
// library-wide exact vertex pool; face/UV seams remain distinct.
struct IndexedMesh {
	StdVector<uint32_t> vertices;
	StdVector<Triangle> triangles;
	bool valid = true;
};

class GeometryTable {
public:
	struct Entry {
		uint64_t shape;
		IndexedMesh mesh;
	};
	bool update(const Settings &settings, bool enabled);
	const IndexedMesh *get(uint64_t shape) const;
	const StdVector<Vertex> &get_vertices() const {
		return _vertices;
	}
	const StdVector<Entry> &get_entries() const {
		return _entries;
	}
	size_t get_storage_bytes() const;

private:
	Settings _settings;
	StdVector<Entry> _entries;
	StdVector<Vertex> _vertices;
};
} // namespace zylann::voxel::blocky::round_geometry
#endif
