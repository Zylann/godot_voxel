#ifndef ZPROFILING_TIMELINE_VIEW_H
#define ZPROFILING_TIMELINE_VIEW_H

#include <scene/gui/control.h>

class ZProfilingClient;
class InputEvent;

// Displays profiling scopes in a timeline
class ZProfilingTimelineView : public Control {
	GDCLASS(ZProfilingTimelineView, Control)
public:
	ZProfilingTimelineView();

	void set_client(const ZProfilingClient *client);
	void set_thread(int thread_index);
	void set_frame_index(int frame_index);

private:
	void _notification(int p_what);
	void _draw();
	void _gui_input(Ref<InputEvent> p_event);

	void add_zoom(float factor, float mouse_x);
	void set_view_range(float min_time_us, float max_time_us);

	static void _bind_methods();

	const ZProfilingClient *_client = nullptr;

	int _thread_index = 0;
	int _frame_index = 0;
	double _view_min_time_us = 0;
	double _view_max_time_us = 1;
};

#endif // ZPROFILING_TIMELINE_VIEW_H
