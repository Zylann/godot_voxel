#ifndef ISLAND_FINDER_H
#define ISLAND_FINDER_H

#include "math/box3i.h"
#include "span.h"

// Scans a grid of binary values and returns another grid
// where all contiguous islands are labelled with a unique ID.
// It is based on a two-pass version of Connected-Component-Labeling.
//
// In the first pass we scan the grid to identify connected chunks by giving them temporary IDs,
// and marking equivalent ones if two chunks touch.
// In the second pass, we replace IDs with consecutive ones starting from 1, which are more convenient to use.
//
// See https://en.wikipedia.org/wiki/Connected-component_labeling
//
class IslandFinder {
public:
	static const int MAX_ISLANDS = 256;

	template <typename VolumePredicate_F>
	void scan_3d(Box3i box, VolumePredicate_F volume_predicate_func, Span<uint8_t> output, unsigned int *out_count) {
		const unsigned int volume = box.size.volume();
		CRASH_COND(output.size() != volume);
		memset(output.data(), 0, volume * sizeof(uint8_t));

		memset(_equivalences.data(), 0, MAX_ISLANDS * sizeof(uint8_t));

		int top_label = 0;
		int left_label = 0;
		int back_label = 0;
		int next_unique_label = 1;

		Vector3i pos;
		for (pos.z = 0; pos.z < box.size.z; ++pos.z) {
			for (pos.x = 0; pos.x < box.size.x; ++pos.x) {
				// TODO I initially wrote this algorithm in ZYX order, but translated to ZXY when porting to C++.
				// `left` means `top`, and `top` means `left`.
				left_label = 0;

				for (pos.y = 0; pos.y < box.size.y; ++pos.y) {
					int label = 0;

					if (volume_predicate_func(box.pos + pos)) {
						if (pos.z > 0) {
							back_label = output[Vector3i(pos.x, pos.y, pos.z - 1).get_zxy_index(box.size)];
						} else {
							back_label = 0;
						}

						if (pos.x > 0) {
							top_label = output[Vector3i(pos.x - 1, pos.y, pos.z).get_zxy_index(box.size)];
						} else {
							top_label = 0;
						}

						// TODO This soup of ifs is the first that worked for me, but there must be a way to simplify

						if (left_label == 0 && top_label == 0 && back_label == 0) {
							// TODO Make the algorithm return instead, it's hard for the caller to handle it otherwise
							CRASH_COND(next_unique_label >= MAX_ISLANDS);
							_equivalences[next_unique_label] = 0;
							label = next_unique_label;
							++next_unique_label;

						} else if (left_label == 0 && top_label == 0) {
							label = back_label;

						} else if (left_label == 0 && back_label == 0) {
							label = top_label;

						} else if (top_label == 0 && back_label == 0) {
							label = left_label;

						} else if (left_label == 0 || (top_label != 0 && back_label != 0 &&
															  (left_label == top_label || left_label == back_label))) {
							if (top_label == back_label) {
								label = back_label;

							} else if (top_label < back_label) {
								label = top_label;
								add_equivalence(back_label, top_label);

							} else {
								label = back_label;
								add_equivalence(top_label, back_label);
							}

						} else if (top_label == 0 || (left_label != 0 && back_label != 0 &&
															 (top_label == left_label || top_label == back_label))) {
							if (left_label == back_label) {
								label = back_label;

							} else if (left_label < back_label) {
								label = left_label;
								add_equivalence(back_label, left_label);

							} else {
								label = back_label;
								add_equivalence(left_label, back_label);
							}

						} else if (back_label == 0 || (left_label != 0 && top_label != 0 &&
															  (back_label == left_label || back_label == top_label))) {
							if (left_label == top_label) {
								label = top_label;

							} else if (left_label < top_label) {
								label = left_label;
								add_equivalence(top_label, left_label);

							} else {
								label = top_label;
								add_equivalence(left_label, top_label);
							}

						} else {
							int a[3] = { left_label, top_label, back_label };
							SortArray<int> sa;
							sa.sort(a, 3);
							label = a[0];
							add_equivalence(a[1], a[0]);
							add_equivalence(a[2], a[1]);
						}

						output[pos.get_zxy_index(box.size)] = label;
					}

					left_label = label;
				}
			}
		}

		flatten_equivalences();
		int count = compact_labels(next_unique_label);

		if (out_count != nullptr) {
			*out_count = count;
		}

		for (unsigned int i = 0; i < output.size(); ++i) {
			uint8_t &c = output[i];
			uint8_t e = _equivalences[c];
			if (e != 0) {
				c = e;
			}
		}
	}

private:
	void add_equivalence(int upper, int lower) {
		CRASH_COND(upper <= lower);
		int prev_lower = _equivalences[upper];

		if (prev_lower == 0) {
			_equivalences[upper] = lower;

		} else if (prev_lower > lower) {
			_equivalences[upper] = lower;
			add_equivalence(prev_lower, lower);

		} else if (prev_lower < lower) {
			add_equivalence(lower, prev_lower);
		}
	}

	// Makes sure equivalences go straight to the label without transitive links
	void flatten_equivalences() {
		for (int i = 1; i < MAX_ISLANDS; ++i) {
			int e = _equivalences[i];
			if (e == 0) {
				continue;
			}
			int e2 = _equivalences[e];
			while (e2 != 0) {
				e = e2;
				e2 = _equivalences[e];
			}
			_equivalences[i] = e;
		}
	}

	// Make sure labels obtained from equivalences are sequential and start from 1.
	// Returns total label count.
	int compact_labels(int equivalences_count) {
		int next_label = 1;
		for (int i = 1; i < equivalences_count; ++i) {
			const int e = _equivalences[i];
			if (e == 0) {
				// That label has no equivalent, give it an index
				_equivalences[i] = next_label;
				next_label += 1;
			} else {
				// That label has an equivalent, give it that index instead
				int e2 = _equivalences[e];
				_equivalences[i] = e2;
			}
		}
		// We started from 1, but end with what would have been the next ID, so we subtract 1 to obtain the count
		return next_label - 1;
	}

private:
	FixedArray<uint8_t, MAX_ISLANDS> _equivalences;
};

#endif // ISLAND_FINDER_H
