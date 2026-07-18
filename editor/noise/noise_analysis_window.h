#ifndef NOISE_ANALYSIS_WINDOW_H
#define NOISE_ANALYSIS_WINDOW_H

#include "../../util/godot/classes/accept_dialog.h"
#include "../../util/noise/fast_noise_2.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "noise_adapter.h"

class SpinBox;
class LineEdit;
class ProgressBar;
class OptionButton;

namespace zylann {

class ZN_ChartView;

// This is an experimental tool to check noise properties empirically,
// by sampling it a lot of times and seeing what the minimum and maximum values are.
class ZN_NoiseAnalysisWindow : public AcceptDialog {
	GDCLASS(ZN_NoiseAnalysisWindow, AcceptDialog)
public:
	ZN_NoiseAnalysisWindow();

	void set_noise(Ref<FastNoise2> noise);
	void set_noise(Ref<ZN_FastNoiseLite> noise);

private:
	enum Dimension { //
		DIMENSION_2D = 0,
		DIMENSION_3D,
		_DIMENSION_COUNT
	};

	void _on_calculate_button_pressed();
	void _notification(int p_what);
	void _process();

	static void _bind_methods();

	NoiseAdapter _adapter;

	OptionButton *_dimension_option_button = nullptr;
	SpinBox *_step_count_spinbox = nullptr;
	SpinBox *_step_minimum_length_spinbox = nullptr;
	SpinBox *_step_maximum_length_spinbox = nullptr;
	SpinBox *_area_size_spinbox = nullptr;
	SpinBox *_samples_count_spinbox = nullptr;

	ZN_ChartView *_chart_view = nullptr;

	ProgressBar *_progress_bar = nullptr;

	LineEdit *_minimum_value_line_edit = nullptr;
	LineEdit *_maximum_value_line_edit = nullptr;
	LineEdit *_maximum_derivative_line_edit = nullptr;

	Button *_calculate_button = nullptr;

	struct AnalysisParams {
		Dimension dimension;
		int step_count;
		float step_minimum_length;
		float step_maximum_length;
		float area_size;
		int samples_count;
	};

	AnalysisParams _analysis_params;
	int _current_step = -1;

	struct AnalysisResults {
		float minimum_value;
		float maximum_value;
		float maximum_derivative;
		PackedVector2Array maximum_derivative_per_step_length;
	};

	AnalysisResults _results;
};

} // namespace zylann

#endif // NOISE_ANALYSIS_WINDOW_H
