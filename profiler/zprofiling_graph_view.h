#ifndef ZPROFILING_GRAPH_VIEW_H
#define ZPROFILING_GRAPH_VIEW_H

#include <scene/gui/control.h>

class ZProfilingClient;

class ZProfilingGraphView : public Control {
	GDCLASS(ZProfilingGraphView, Control)
public:
	ZProfilingGraphView();

	void set_client(ZProfilingClient *client);

private:
	void _notification(int p_what);
	void _draw();

	static void _bind_methods();

	const ZProfilingClient *_client = nullptr;
};

#endif // ZPROFILING_GRAPH_VIEW_H
