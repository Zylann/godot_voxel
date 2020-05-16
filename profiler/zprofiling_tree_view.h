#ifndef ZPROFILING_TREE_VIEW_H
#define ZPROFILING_TREE_VIEW_H

#include <scene/gui/control.h>

class Tree;
class ZProfilingClient;

class ZProfilingTreeView : public Control {
	GDCLASS(ZProfilingTreeView, Control)
public:
	ZProfilingTreeView();

	void set_client(const ZProfilingClient *client);
	void set_frame_index(int frame_index);
	void set_thread_index(int thread_index);

	enum Column {
		COLUMN_NAME = 0,
		COLUMN_HIT_COUNT,
		COLUMN_TOTAL_TIME,
		COLUMN_COUNT
	};

private:
	static void _bind_methods();

	void update_tree();
	void sort_tree();

	const ZProfilingClient *_client;
	Tree *_tree = nullptr;
	int _thread_index;
	int _frame_index;
};

#endif // ZPROFILING_TREE_VIEW_H
