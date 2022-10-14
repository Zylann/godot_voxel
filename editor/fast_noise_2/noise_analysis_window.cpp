#include "noise_analysis_window.h"
#include "../../util/godot/funcs.h"
#include "chart_view.h"

#include <editor/editor_scale.h>
#include <scene/gui/box_container.h>
#include <scene/gui/grid_container.h>
#include <scene/gui/option_button.h>
#include <scene/gui/progress_bar.h>
#include <scene/gui/spin_box.h>

namespace zylann {

NoiseAnalysisWindow::NoiseAnalysisWindow() {
	set_title(TTR("Noise Analysis"));
	set_min_size(Vector2(300.f * EDSCALE, 0));

	VBoxContainer *vbox_container = memnew(VBoxContainer);

	{
		GridContainer *gc1 = memnew(GridContainer);
		gc1->set_columns(2);

		{
			Label *label = memnew(Label);
			label->set_text(TTR("Dimension"));
			gc1->add_child(label);

			_dimension_option_button = memnew(OptionButton);
			_dimension_option_button->get_popup()->add_item(TTR("2D"), DIMENSION_2D);
			_dimension_option_button->get_popup()->add_item(TTR("3D"), DIMENSION_3D);
			_dimension_option_button->select(0);
			gc1->add_child(_dimension_option_button);
		}
		{
			Label *label = memnew(Label);
			label->set_text(TTR("Step count"));
			gc1->add_child(label);

			_step_count_spinbox = memnew(SpinBox);
			_step_count_spinbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			_step_count_spinbox->set_min(1);
			_step_count_spinbox->set_max(100);
			_step_count_spinbox->set_step(1.0);
			_step_count_spinbox->set_value(10);
			gc1->add_child(_step_count_spinbox);
		}
		{
			Label *label = memnew(Label);
			label->set_text(TTR("Step minimum length"));
			gc1->add_child(label);

			_step_minimum_length_spinbox = memnew(SpinBox);
			_step_minimum_length_spinbox->set_min(0.001);
			_step_minimum_length_spinbox->set_max(1.0);
			_step_minimum_length_spinbox->set_step(0.001);
			_step_minimum_length_spinbox->set_value(0.01);
			gc1->add_child(_step_minimum_length_spinbox);
		}
		{
			Label *label = memnew(Label);
			label->set_text(TTR("Step maximum length"));
			gc1->add_child(label);

			_step_maximum_length_spinbox = memnew(SpinBox);
			_step_maximum_length_spinbox->set_min(0.001);
			_step_maximum_length_spinbox->set_max(1.0);
			_step_maximum_length_spinbox->set_step(0.001);
			_step_maximum_length_spinbox->set_value(0.1);
			gc1->add_child(_step_maximum_length_spinbox);
		}
		{
			Label *label = memnew(Label);
			label->set_text(TTR("Area size"));
			gc1->add_child(label);

			_area_size_spinbox = memnew(SpinBox);
			_area_size_spinbox->set_min(0.0);
			_area_size_spinbox->set_max(100000.0);
			_area_size_spinbox->set_step(1.0);
			_area_size_spinbox->set_value(4000);
			gc1->add_child(_area_size_spinbox);
		}
		{
			Label *label = memnew(Label);
			label->set_text(TTR("Samples count"));
			gc1->add_child(label);

			_samples_count_spinbox = memnew(SpinBox);
			_samples_count_spinbox->set_min(1);
			_samples_count_spinbox->set_max(100000);
			_samples_count_spinbox->set_step(1);
			_samples_count_spinbox->set_value(10000);
			gc1->add_child(_samples_count_spinbox);
		}
		vbox_container->add_child(gc1);
	}
	{
		_calculate_button = memnew(Button);
		_calculate_button->set_text(TTR("Calculate"));
		_calculate_button->connect("pressed", callable_mp(this, &NoiseAnalysisWindow::_on_calculate_button_pressed));
		vbox_container->add_child(_calculate_button);
	}

	_progress_bar = memnew(ProgressBar);
	_progress_bar->set_value(0);
	vbox_container->add_child(_progress_bar);

	{
		Label *label = memnew(Label);
		label->set_text(TTR("Maximum derivative over step length*:"));
		label->set_tooltip_text(
				TTR("Depending on the noise type, this measure can vary due to very small discontinuities, "
					"so it may be interesting to try multiple step lengths, from shortest to longest."));
		label->set_mouse_filter(Control::MOUSE_FILTER_STOP);
		vbox_container->add_child(label);
	}

	_chart_view = memnew(ChartView);
	_chart_view->set_custom_minimum_size(Vector2(0, 150.0 * EDSCALE));
	_chart_view->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vbox_container->add_child(_chart_view);

	{
		GridContainer *gc2 = memnew(GridContainer);
		gc2->set_columns(2);

		{
			Label *label = memnew(Label);
			label->set_text(TTR("Minimum value"));
			gc2->add_child(label);

			_minimum_value_line_edit = memnew(LineEdit);
			_minimum_value_line_edit->set_editable(false);
			_minimum_value_line_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			gc2->add_child(_minimum_value_line_edit);
		}
		{
			Label *label = memnew(Label);
			label->set_text(TTR("Maximum value"));
			gc2->add_child(label);

			_maximum_value_line_edit = memnew(LineEdit);
			_maximum_value_line_edit->set_editable(false);
			gc2->add_child(_maximum_value_line_edit);
		}
		{
			Label *label = memnew(Label);
			label->set_text(TTR("Maximum derivative"));
			gc2->add_child(label);

			_maximum_derivative_line_edit = memnew(LineEdit);
			_maximum_derivative_line_edit->set_editable(false);
			gc2->add_child(_maximum_derivative_line_edit);
		}

		vbox_container->add_child(gc2);
	}

	add_child(vbox_container);
}

void NoiseAnalysisWindow::set_noise(Ref<FastNoise2> noise) {
	_noise = noise;
}

namespace {
std::vector<Vector2> &get_tls_precomputed_unit_vectors_2d() {
	static thread_local std::vector<Vector2> tls_precomputed_unit_vectors_2d;
	return tls_precomputed_unit_vectors_2d;
}
std::vector<Vector3> &get_tls_precomputed_unit_vectors_3d() {
	static thread_local std::vector<Vector3> tls_precomputed_unit_vectors_3d;
	return tls_precomputed_unit_vectors_3d;
}
} //namespace

void NoiseAnalysisWindow::_on_calculate_button_pressed() {
	ERR_FAIL_COND(_noise.is_null());

	_analysis_params.dimension = Dimension(_dimension_option_button->get_selected_id());
	ERR_FAIL_INDEX(_analysis_params.dimension, _DIMENSION_COUNT);
	_analysis_params.area_size = _area_size_spinbox->get_value();
	_analysis_params.samples_count = int(_samples_count_spinbox->get_value());
	_analysis_params.step_count = int(_step_count_spinbox->get_value());
	_analysis_params.step_minimum_length = _step_minimum_length_spinbox->get_value();
	_analysis_params.step_maximum_length = _step_maximum_length_spinbox->get_value();

	ERR_FAIL_COND(_analysis_params.step_count <= 0);

	// Clear previous results
	_results.maximum_derivative_per_step_length.clear();
	_results.maximum_derivative_per_step_length.resize(_analysis_params.step_count);
	_results.maximum_derivative = 0.f;
	if (_analysis_params.dimension == DIMENSION_2D) {
		_results.minimum_value = _noise->get_noise_2d_single(Vector2());
	} else {
		_results.minimum_value = _noise->get_noise_3d_single(Vector3());
	}
	_results.maximum_value = _results.minimum_value;

	_current_step = 0;

	// Precompute unit vectors
	const int precomputed_vectors_count = 256;
	if (_analysis_params.dimension == DIMENSION_2D) {
		std::vector<Vector2> &precomputed_unit_vectors_2d = get_tls_precomputed_unit_vectors_2d();
		precomputed_unit_vectors_2d.resize(precomputed_vectors_count);
		for (int i = 0; i < precomputed_vectors_count; ++i) {
			const float a = Math_TAU * float(i) / float(precomputed_vectors_count);
			precomputed_unit_vectors_2d[i] = Vector2(Math::cos(a), Math::sin(a));
		}
	} else {
		std::vector<Vector3> &precomputed_unit_vectors_3d = get_tls_precomputed_unit_vectors_3d();
		precomputed_unit_vectors_3d.resize(precomputed_vectors_count);
		for (int i = 0; i < precomputed_vectors_count; ++i) {
			// TODO Uniform repartition of 3D vectors?
			precomputed_unit_vectors_3d[i] =
					Vector3(Math::random(-1.f, 1.f), Math::random(-1.f, 1.f), Math::random(-1.f, 1.f)).normalized();
		}
	}

	_progress_bar->set_value(0);

	_calculate_button->set_disabled(true);

	set_process(true);
}

void NoiseAnalysisWindow::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:
			_process();
			break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			if (!is_visible()) {
				// Release reference when the window is closed
				set_noise(Ref<FastNoise2>());
			}
			break;

		default:
			break;
	}
}

void NoiseAnalysisWindow::_process() {
	ERR_FAIL_COND(_noise.is_null());
	ERR_FAIL_COND(_analysis_params.step_count <= 0);

	if (!is_processing()) {
		return;
	}

	const float step_length = Math::lerp(_analysis_params.step_minimum_length, _analysis_params.step_maximum_length,
			float(_current_step) / _analysis_params.step_count);

	std::vector<float> x_cache;
	std::vector<float> y_cache;
	std::vector<float> z_cache;
	std::vector<float> noise_cache;
	std::vector<float> noise_cache2;

	x_cache.clear();
	y_cache.clear();
	z_cache.clear();

	x_cache.resize(_analysis_params.samples_count);
	y_cache.resize(_analysis_params.samples_count);
	z_cache.resize(_analysis_params.samples_count);

	noise_cache.resize(_analysis_params.samples_count);
	noise_cache2.resize(_analysis_params.samples_count);

	if (_analysis_params.dimension == DIMENSION_2D) {
		// 2D
		for (int i = 0; i < _analysis_params.samples_count; ++i) {
			const float x = Math::random(-_analysis_params.area_size, _analysis_params.area_size);
			const float y = Math::random(-_analysis_params.area_size, _analysis_params.area_size);
			x_cache[i] = x;
			y_cache[i] = y;
		}

		_noise->get_noise_2d_series(to_span_const(x_cache), to_span_const(y_cache), to_span(noise_cache));

		std::vector<Vector2> &precomputed_unit_vectors_2d = get_tls_precomputed_unit_vectors_2d();

		for (size_t i = 0; i < noise_cache.size(); ++i) {
			const Vector2 u = step_length * precomputed_unit_vectors_2d[i % precomputed_unit_vectors_2d.size()];
			x_cache[i] += u.x;
			y_cache[i] += u.y;
		}

		_noise->get_noise_2d_series(to_span_const(x_cache), to_span_const(y_cache), to_span(noise_cache2));

	} else {
		// 3D
		for (int i = 0; i < _analysis_params.samples_count; ++i) {
			const float x = Math::random(-_analysis_params.area_size, _analysis_params.area_size);
			const float y = Math::random(-_analysis_params.area_size, _analysis_params.area_size);
			const float z = Math::random(-_analysis_params.area_size, _analysis_params.area_size);
			x_cache[i] = x;
			y_cache[i] = y;
			z_cache[i] = z;
		}

		std::vector<Vector3> &precomputed_unit_vectors_3d = get_tls_precomputed_unit_vectors_3d();

		_noise->get_noise_3d_series(
				to_span_const(x_cache), to_span_const(y_cache), to_span_const(z_cache), to_span(noise_cache));

		for (size_t i = 0; i < noise_cache.size(); ++i) {
			const Vector3 u = step_length * precomputed_unit_vectors_3d[i % precomputed_unit_vectors_3d.size()];
			x_cache[i] += u.x;
			y_cache[i] += u.y;
			z_cache[i] += u.z;
		}

		_noise->get_noise_3d_series(
				to_span_const(x_cache), to_span_const(y_cache), to_span_const(z_cache), to_span(noise_cache2));
	}

	float max_derivative = 0.f;
	for (size_t i = 0; i < noise_cache.size(); ++i) {
		_results.minimum_value = math::min(_results.minimum_value, noise_cache[i]);
		_results.maximum_value = math::max(_results.maximum_value, noise_cache[i]);
		const float derivative = Math::abs((noise_cache2[i] - noise_cache[i]) / step_length);
		max_derivative = math::max(derivative, max_derivative);
	}

	_results.maximum_derivative = math::max(max_derivative, _results.maximum_derivative);
	_results.maximum_derivative_per_step_length.write[_current_step] = Vector2(step_length, max_derivative);

	++_current_step;

	_progress_bar->set_value(100.f * float(_current_step) / float(_analysis_params.step_count));

	if (_current_step >= _analysis_params.step_count) {
		set_process(false);

		_chart_view->set_points(to_span(_results.maximum_derivative_per_step_length));
		_chart_view->auto_fit_view(Vector2(0.1, 0.1));

		_minimum_value_line_edit->set_text(String::num_real(_results.minimum_value));
		_maximum_value_line_edit->set_text(String::num_real(_results.maximum_value));
		_maximum_derivative_line_edit->set_text(String::num_real(_results.maximum_derivative));

		_calculate_button->set_disabled(false);
	}
}

void NoiseAnalysisWindow::_bind_methods() {}

} // namespace zylann
