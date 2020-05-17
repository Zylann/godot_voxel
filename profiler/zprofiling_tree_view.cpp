#include "zprofiling_tree_view.h"
#include "zprofiling_client.h"
#include "zprofiling_server.h"

#include <scene/gui/tree.h>

ZProfilingTreeView::ZProfilingTreeView() {
	_tree = memnew(Tree);
	_tree->set_anchors_and_margins_preset(PRESET_WIDE);
	_tree->set_hide_root(true);
	_tree->set_columns(COLUMN_COUNT);
	_tree->set_column_titles_visible(true);
	_tree->set_column_title(COLUMN_NAME, "Location");
	_tree->set_column_title(COLUMN_HIT_COUNT, "Hits");
	_tree->set_column_title(COLUMN_TOTAL_TIME, "Total ms");
	_tree->set_column_expand(COLUMN_HIT_COUNT, false);
	_tree->set_column_expand(COLUMN_TOTAL_TIME, false);
	_tree->set_column_min_width(COLUMN_HIT_COUNT, 50);
	_tree->set_column_min_width(COLUMN_TOTAL_TIME, 75);
	add_child(_tree);
}

void ZProfilingTreeView::set_client(const ZProfilingClient *client) {
	_client = client;
}

void ZProfilingTreeView::set_frame_index(int frame_index) {
	if (_frame_index != frame_index) {
		_frame_index = frame_index;
		update_tree();
	}
}

void ZProfilingTreeView::set_thread_index(int thread_index) {
	if (_thread_index != thread_index) {
		_thread_index = thread_index;
		update_tree();
	}
}

static void process_item(
		const ZProfilingClient::Item &item,
		const ZProfilingClient::Frame *frame, int lane_index,
		const ZProfilingClient *client, Tree *tree, TreeItem *parent_tree_item,
		std::array<int, ZProfiler::MAX_STACK> &lane_tails) {

	// Search for existing item
	TreeItem *tree_item = parent_tree_item->get_children();
	while (tree_item != nullptr) {
		const uint16_t string_id = tree_item->get_metadata(ZProfilingTreeView::COLUMN_NAME);
		if (string_id == item.description_id) {
			break;
		}
		tree_item = tree_item->get_next();
	}

	if (tree_item != nullptr) {
		// First occurrence
		int hit_count = tree_item->get_metadata(ZProfilingTreeView::COLUMN_HIT_COUNT);
		int total_time = tree_item->get_metadata(ZProfilingTreeView::COLUMN_TOTAL_TIME);

		++hit_count;
		total_time += item.end_time_relative - item.begin_time_relative;

		tree_item->set_metadata(ZProfilingTreeView::COLUMN_HIT_COUNT, hit_count);
		tree_item->set_metadata(ZProfilingTreeView::COLUMN_TOTAL_TIME, total_time);

	} else {
		// More occurrences
		tree_item = tree->create_item(parent_tree_item);

		String text = client->get_indexed_name(item.description_id);
		if (text.length() > 32) {
			tree_item->set_tooltip(ZProfilingTreeView::COLUMN_NAME, text);
			text = String("...") + text.right(text.length() - 32);
		}
		tree_item->set_text(ZProfilingTreeView::COLUMN_NAME, text);

		tree_item->set_metadata(ZProfilingTreeView::COLUMN_NAME, item.description_id);
		tree_item->set_metadata(ZProfilingTreeView::COLUMN_HIT_COUNT, 1);
		tree_item->set_metadata(ZProfilingTreeView::COLUMN_TOTAL_TIME, item.end_time_relative - item.begin_time_relative);
	}

	// Process children in the same time range as the parent
	int next_lane_index = lane_index + 1;
	if (next_lane_index < frame->lanes.size()) {
		int child_item_index = lane_tails[next_lane_index];
		const ZProfilingClient::Lane &next_lane = frame->lanes[next_lane_index];

		while (child_item_index < next_lane.items.size()) {
			const ZProfilingClient::Item &child_item = next_lane.items[child_item_index];
			CRASH_COND(child_item.begin_time_relative < item.begin_time_relative);
			if (child_item.begin_time_relative > item.end_time_relative) {
				break;
			}
			process_item(child_item, frame, next_lane_index, client, tree, tree_item, lane_tails);
			++child_item_index;
		}

		// Remember last child index so we'll start from there when processing the next item in our lane
		lane_tails[next_lane_index] = child_item_index;
	}
}

static void update_tree_text(TreeItem *tree_item) {
	const int hit_count = tree_item->get_metadata(ZProfilingTreeView::COLUMN_HIT_COUNT);
	const int total_time = tree_item->get_metadata(ZProfilingTreeView::COLUMN_TOTAL_TIME);

	tree_item->set_text(ZProfilingTreeView::COLUMN_HIT_COUNT, String::num_int64(hit_count));
	tree_item->set_text(ZProfilingTreeView::COLUMN_TOTAL_TIME, String::num_real((float)total_time / 1000.0));

	TreeItem *child = tree_item->get_children();
	while (child != nullptr) {
		update_tree_text(child);
		child = child->get_next();
	}
}

template <typename Comparator_T>
static void sort_tree(TreeItem *parent_tree_item) {
	Vector<TreeItem *> items;

	TreeItem *item = parent_tree_item->get_children();
	while (item != nullptr) {
		// Sort recursively
		sort_tree<Comparator_T>(item);

		items.push_back(item);
		item = item->get_next();
	}

	items.sort_custom<Comparator_T>();

	for (int i = items.size() - 1; i >= 0; --i) {
		// Moving to top is less expensive
		items[i]->move_to_top();
	}
}

void ZProfilingTreeView::update_tree() {
	ERR_FAIL_COND(_client == nullptr);
	const ZProfilingClient::Frame *frame = _client->get_frame(_thread_index, _frame_index);
	if (frame == nullptr) {
		return;
	}

	_tree->clear();
	TreeItem *tree_root = _tree->create_item();

	if (frame->lanes.size() == 0) {
		return;
	}

	std::array<int, ZProfiler::MAX_STACK> lane_tails;
	for (int i = 0; i < lane_tails.size(); ++i) {
		lane_tails[i] = 0;
	}

	int lane_index = 0;
	const ZProfilingClient::Lane &lane = frame->lanes[lane_index];

	for (int item_index = 0; item_index < lane.items.size(); ++item_index) {
		const ZProfilingClient::Item &item = lane.items[item_index];
		process_item(item, frame, lane_index, _client, _tree, tree_root, lane_tails);
	}

	TreeItem *child = tree_root->get_children();
	while (child != nullptr) {
		update_tree_text(child);
		child = child->get_next();
	}

	sort_tree();
}

void ZProfilingTreeView::sort_tree() {
	struct TimeComparator {
		bool operator()(const TreeItem *a, const TreeItem *b) const {
			const int a_total = a->get_metadata(ZProfilingTreeView::COLUMN_TOTAL_TIME);
			const int b_total = b->get_metadata(ZProfilingTreeView::COLUMN_TOTAL_TIME);
			return a_total > b_total;
		}
	};

	// TODO Different sorters
	::sort_tree<TimeComparator>(_tree->get_root());
}

void ZProfilingTreeView::_bind_methods() {
}
