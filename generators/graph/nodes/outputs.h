#include "../node_type_db.h"

namespace zylann::voxel::pg {

void register_output_nodes(Span<NodeType> types) {
	using namespace math;

	{
		NodeType &t = types[VoxelGraphFunction::NODE_OUTPUT_SDF];
		t.name = "OutputSDF";
		t.category = CATEGORY_OUTPUT;
		t.inputs.push_back(NodeType::Port("sdf", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.outputs.push_back(NodeType::Port("_out"));
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			const Runtime::Buffer &input = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			ZN_ASSERT(out.data != nullptr);
			memcpy(out.data, input.data, input.size * sizeof(float));
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, a);
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_OUTPUT_WEIGHT];
		t.name = "OutputWeight";
		t.category = CATEGORY_OUTPUT;
		t.inputs.push_back(NodeType::Port("weight"));
		t.outputs.push_back(NodeType::Port("_out"));
		NodeType::Param layer_param("layer", Variant::INT, 0);
		layer_param.has_range = true;
		layer_param.min_value = 0;
		layer_param.max_value = 15;
		t.params.push_back(layer_param);
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			const Runtime::Buffer &input = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			for (unsigned int i = 0; i < out.size; ++i) {
				out.data[i] = clamp(input.data[i], 0.f, 1.f);
			}
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, clamp(a, Interval::from_single_value(0.f), Interval::from_single_value(1.f)));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_OUTPUT_TYPE];
		t.name = "OutputType";
		t.category = CATEGORY_OUTPUT;
		t.inputs.push_back(NodeType::Port("type"));
		t.outputs.push_back(NodeType::Port("_out"));
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			const Runtime::Buffer &input = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			memcpy(out.data, input.data, input.size * sizeof(float));
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, a);
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_OUTPUT_SINGLE_TEXTURE];
		t.name = "OutputSingleTexture";
		t.category = CATEGORY_OUTPUT;
		t.inputs.push_back(NodeType::Port("index"));
		t.outputs.push_back(NodeType::Port("_out"));
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			const Runtime::Buffer &input = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			memcpy(out.data, input.data, input.size * sizeof(float));
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, a);
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_CUSTOM_OUTPUT];
		t.name = "CustomOutput";
		t.category = CATEGORY_OUTPUT;
		t.inputs.push_back(NodeType::Port("value"));
		t.outputs.push_back(NodeType::Port("_out"));
		// t.params.push_back(NodeType::Param("binding", Variant::INT, 0));
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			const Runtime::Buffer &input = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			memcpy(out.data, input.data, input.size * sizeof(float));
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, a);
		};
	}
}

} // namespace zylann::voxel::pg
