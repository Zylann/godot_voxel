#ifndef VOXEL_TRANSVOXEL_CELL_ITERATOR_H
#define VOXEL_TRANSVOXEL_CELL_ITERATOR_H

#include "../../engine/detail_rendering.h"
#include "transvoxel.h"

namespace zylann::voxel {

// Implement the generic interface to iterate voxel mesh cells, which can be used to compute virtual textures.
// This one is optimized to gather results of the Transvoxel mesher, which comes with cell information out of the box.
class TransvoxelCellIterator : public ICellIterator {
public:
	TransvoxelCellIterator(Span<const transvoxel::CellInfo> p_cell_infos) :
			_current_index(0), _triangle_begin_index(0) {
		// Make a copy
		_cell_infos.resize(p_cell_infos.size());
		for (unsigned int i = 0; i < p_cell_infos.size(); ++i) {
			_cell_infos[i] = p_cell_infos[i];
		}
	}

	unsigned int get_count() const override {
		return _cell_infos.size();
	}

	bool next(CurrentCellInfo &current) override {
		if (_current_index < _cell_infos.size()) {
			const transvoxel::CellInfo &cell = _cell_infos[_current_index];
			current.position = cell.position;
			current.triangle_count = cell.triangle_count;

			for (unsigned int i = 0; i < cell.triangle_count; ++i) {
				current.triangle_begin_indices[i] = _triangle_begin_index;
				_triangle_begin_index += 3;
			}

			++_current_index;
			return true;

		} else {
			return false;
		}
	}

	void rewind() {
		_current_index = 0;
		_triangle_begin_index = 0;
	}

private:
	std::vector<transvoxel::CellInfo> _cell_infos;
	unsigned int _current_index;
	unsigned int _triangle_begin_index;
};

} // namespace zylann::voxel

#endif // VOXEL_TRANSVOXEL_CELL_ITERATOR_H
