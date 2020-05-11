#ifndef ZPROFILING_CLIENT_FLAME_VIEW_H
#define ZPROFILING_CLIENT_FLAME_VIEW_H

#include <scene/gui/control.h>

class ZProfilingClient;
class InputEvent;

// Displays profiling scopes in a timeline
class ZProfilingClientFlameView : public Control {
	GDCLASS(ZProfilingClientFlameView, Control)
public:
	ZProfilingClientFlameView();

	void set_client(const ZProfilingClient *client);
	void set_frame(int frame_index);
	void set_thread(int thread_index);

private:
	void _notification(int p_what);
	void _draw();
	void _gui_input(Ref<InputEvent> p_event);

	static void _bind_methods();

	const ZProfilingClient *_client = nullptr;

	int _frame_index = 0;
	int _thread_index = 0;

	// TODO View window
	//	int _min_time = -1;
	//	int _max_time = -1;
};

#endif // ZPROFILING_CLIENT_FLAME_VIEW_H
