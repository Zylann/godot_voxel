#include "a_star_grid_3d.h"
#include "../util/container_funcs.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"
#include <unordered_set>

namespace zylann {

AStarGrid3D::AStarGrid3D() {
	_open_list.sorter.compare.pool = &_points_pool;
}

void AStarGrid3D::set_region(Box3i region) {
	_region = region;
}

void AStarGrid3D::set_agent_size(Vector3f size) {
	ZN_ASSERT_RETURN(math::is_valid_size(size));
	_agent_size = size;
}

void AStarGrid3D::set_max_fall_height(int h) {
	_max_fall_height = math::max(h, 0);
}

int AStarGrid3D::get_max_fall_height() const {
	return _max_fall_height;
}

void AStarGrid3D::set_max_path_cost(float cost) {
	_max_path_cost = math::max(cost, 0.f);
}

float AStarGrid3D::get_max_path_cost() const {
	return _max_path_cost;
}

void AStarGrid3D::start(Vector3i from_position, Vector3i target_position) {
	clear();

	_target_position = target_position;

	_fitting_offset = Vector3f( //
			(int(_agent_size.x) & 1) == 1 ? 0.5f : 0.f, //
			(int(_agent_size.y) & 1) == 1 ? 0.5f : 0.f, //
			(int(_agent_size.z) & 1) == 1 ? 0.5f : 0.f);

	if (!_region.contains(from_position)) {
		return;
	}
	if (!_region.contains(target_position)) {
		return;
	}

	Point src_node;
	src_node.position = from_position;
	src_node.gscore = 0.f;
	src_node.fscore = evaluate_heuristic(from_position, target_position);
	src_node.came_from_point_index = Point::NO_CAME_FROM;
	src_node.in_open_set = true;

	const uint32_t point_index = _points_pool.size();
	_start_point_index = point_index;
	_points_pool.push_back(src_node);

	_open_list.push(point_index);

	_points_map.insert({ from_position, point_index });

	_is_running = true;
}

void AStarGrid3D::step() {
	ZN_PROFILE_SCOPE();

	if (!_is_running) {
		return;
	}

	if (_open_list.size() == 0) {
		_is_running = false;
		_path.clear();
		return;
	}

	_open_list.sorter.compare.pool = &_points_pool;
	const uint32_t current_point_index = _open_list.peek();

	Point current_point = _points_pool[current_point_index];

	if (current_point.position == _target_position) {
		reconstruct_path(current_point_index);
		_is_running = false;
		return;
	}

	// Remove the current point from the open list.
	_open_list.pop();

	_points_pool[current_point_index].in_open_set = false;

	_neighbor_positions.clear();
	get_neighbor_positions(current_point.position, _neighbor_positions);

	for (const Vector3i npos : _neighbor_positions) {
		uint32_t neighbor_point_index;
		auto it = _points_map.find(npos);

		if (it != _points_map.end()) {
			neighbor_point_index = it->second;

		} else {
			neighbor_point_index = _points_pool.size();

			Point p;
			p.position = npos;
			// We haven't visited this node yet so it's not calculated.
			// We set it to infinite (large) number so it will naturally be higher than the computed gscore and will
			// get pushed to the open list
			p.gscore = 999999999.f;
			p.fscore = 0.f;
			p.came_from_point_index = Point::NO_CAME_FROM;
			p.in_open_set = false;

			_points_pool.push_back(p);
			// ZN_PROFILE_SCOPE_NAMED("Insert to points map");
			_points_map.insert({ npos, neighbor_point_index });
		}

		const Vector3i neighbor_dir = npos - current_point.position;
		const float edge_cost = math::length(to_vec3f(neighbor_dir)); // neighbor_dir.length();
		const float tentative_gscore = current_point.gscore + edge_cost;

		Point &neighbor_point = _points_pool[neighbor_point_index];

		// Adding an epsilon to fix float precision issues, which can cause all visited nodes to get
		// visited again due to a tiny difference, doubling search time in some cases. Normally, cost between nodes
		// increases way more than this epsilon.
		const float epsilon = 0.001f;

		if (tentative_gscore + epsilon < neighbor_point.gscore && tentative_gscore < _max_path_cost) {
			neighbor_point.came_from_point_index = current_point_index;

			neighbor_point.gscore = tentative_gscore;
			neighbor_point.fscore = tentative_gscore + evaluate_heuristic(npos, _target_position);

			if (!neighbor_point.in_open_set) {
				_open_list.push(neighbor_point_index);
				neighbor_point.in_open_set = true;

			} else {
				_open_list.update_priority(neighbor_point_index);
			}
		}
	}
}

bool AStarGrid3D::is_solid(Vector3i pos) {
	// Implemented in subclasses
	return false;
}

bool AStarGrid3D::is_ground_close_enough(Vector3i pos) {
	for (int i = 0; i < _max_fall_height; ++i) {
		const Vector3i below_pos = pos - Vector3i(0, i + 1, 0);
		if (!_region.contains(below_pos)) {
			return false;
		}
		const bool below_c = is_solid(below_pos);
		if (below_c) {
			return true;
		}
	}
	return false;
}

bool AStarGrid3D::fits(Vector3f pos, Vector3f agent_extents) {
	const Box3i box = Box3i::from_min_max( //
			to_vec3i(math::floor(pos - agent_extents)), //
			to_vec3i(math::ceil(pos + agent_extents)))
							  .clipped(_region);
	return box.all_cells_match([this](const Vector3i ipos) { return !is_solid(ipos); });
}

namespace {
const Vector3i g_directions_2d[8] = {
	Vector3i(-1, 0, -1),
	Vector3i(0, 0, -1),
	Vector3i(1, 0, -1),
	Vector3i(-1, 0, 0),
	Vector3i(1, 0, 0),
	Vector3i(-1, 0, 1),
	Vector3i(0, 0, 1),
	Vector3i(1, 0, 1),
};
}

void AStarGrid3D::get_neighbor_positions(Vector3i pos, std::vector<Vector3i> &out_positions) {
	// Implementation specialized for agents walking on top of solid surfaces

	ZN_PROFILE_SCOPE();

	const Vector3i pos_below = pos - Vector3i(0, 1, 0);
	const bool c_below = is_solid(pos_below);

	bool may_jump = false;

	for (unsigned int dir_index = 0; dir_index < 8; ++dir_index) {
		const Vector3i npos = pos + g_directions_2d[dir_index];

		if (!_region.contains(npos)) {
			continue;
		}

		const bool nc = is_solid(npos);
		if (nc) {
			may_jump = true;
			continue;

		} else {
			if (c_below == false) {
				// Coming from a "floating" position (jump?)
				const Vector3i npos_below = npos - Vector3i(0, 1, 0);
				if (!_region.contains(npos_below)) {
					continue;
				}
				const bool npos_below_c = is_solid(npos_below);
				if (npos_below_c == false) {
					// Can't fly
					continue;
				}
			}
			if (!is_ground_close_enough(npos)) {
				continue;
			}
		}

		out_positions.push_back(npos);
	}

	if (may_jump) {
		if (c_below) {
			const Vector3i npos = pos + Vector3i(0, 1, 0);
			if (_region.contains(npos)) {
				out_positions.push_back(npos);
			}
		}
	}

	if (c_below == false) {
		const Vector3i npos = pos - Vector3i(0, 1, 0);
		if (_region.contains(npos)) {
			// Fall
			out_positions.push_back(npos);
		}
	}

	{
		// ZN_PROFILE_SCOPE_NAMED("Agent fitting checks");
		// TODO This takes half of time in profiled results

		unordered_remove_if(out_positions, [this, pos](Vector3i npos) {
			// Check if fits the target cell
			if (!fits(to_vec3f(npos) + Vector3f(0.5f) + _fitting_offset, _agent_size * 0.5f)) {
				return true;
			}
			// Check if fits between the source and destination cells
			const Vector3f midp = to_vec3f(pos + npos + Vector3(1, 1, 1)) * 0.5f;
			if (!fits(midp + _fitting_offset, _agent_size * 0.5f)) {
				return true;
			}
			return false;
		});
	}
}

void AStarGrid3D::reconstruct_path(uint32_t end_point_index) {
	ZN_PROFILE_SCOPE();

	_path.clear();

	unsigned int i = 0;
	unsigned int point_index = end_point_index;

	while (point_index != _start_point_index) {
		const uint32_t came_from_index = _points_pool[point_index].came_from_point_index;
		ZN_ASSERT_RETURN(came_from_index != Point::NO_CAME_FROM);
		point_index = came_from_index;
		const Vector3i pos = _points_pool[point_index].position;
		_path.push_back(pos);
		++i;
		ZN_ASSERT_RETURN_MSG(i < 10000, "Too many iterations");
	}

	std::reverse(_path.begin(), _path.end());
}

bool AStarGrid3D::is_running() const {
	return _is_running;
}

Span<const Vector3i> AStarGrid3D::get_path() const {
	return to_span(_path);
}

void AStarGrid3D::clear() {
	_open_list.clear();
	_points_map.clear();
	_points_pool.clear();
	_path.clear();
	_neighbor_positions.clear();
	_is_running = false;
}

float AStarGrid3D::evaluate_heuristic(Vector3i pos, Vector3i target_pos) const {
	const Vector3i diff = target_pos - pos;
	// Manhattan
	return Math::abs(diff.x) + Math::abs(diff.y) + Math::abs(diff.z);
}

void AStarGrid3D::debug_get_visited_points(std::vector<Vector3i> &out_positions) const {
	out_positions.reserve(out_positions.size() + _points_map.size());
	for (auto it = _points_map.begin(); it != _points_map.end(); ++it) {
		out_positions.push_back(it->first);
	}
}

bool AStarGrid3D::debug_get_next_step_point(Vector3i &out_pos) const {
	if (_open_list.size() == 0) {
		return false;
	}
	const uint32_t i = _open_list.peek();
	out_pos = _points_pool[i].position;
	return true;
}

} // namespace zylann
