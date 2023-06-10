#ifndef VOXEL_GRAPH_NODES_UTIL_H
#define VOXEL_GRAPH_NODES_UTIL_H

#include "../voxel_graph_runtime.h"

namespace zylann::voxel::pg {

template <typename F>
inline void do_monop(pg::Runtime::ProcessBufferContext &ctx, F f) {
	const Runtime::Buffer &a = ctx.get_input(0);
	Runtime::Buffer &out = ctx.get_output(0);
	if (a.is_constant) {
		// Normally this case should have been optimized out at compile-time
		const float v = f(a.constant_value);
		for (uint32_t i = 0; i < a.size; ++i) {
			out.data[i] = v;
		}
	} else {
		for (uint32_t i = 0; i < a.size; ++i) {
			out.data[i] = f(a.data[i]);
		}
	}
}

template <typename F>
inline void do_binop(pg::Runtime::ProcessBufferContext &ctx, F f) {
	const Runtime::Buffer &a = ctx.get_input(0);
	const Runtime::Buffer &b = ctx.get_input(1);
	Runtime::Buffer &out = ctx.get_output(0);
	const uint32_t buffer_size = out.size;

	if (a.is_constant || b.is_constant) {
		if (!b.is_constant) {
			const float c = a.constant_value;
			const float *v = b.data;
			for (uint32_t i = 0; i < buffer_size; ++i) {
				out.data[i] = f(c, v[i]);
			}

		} else if (!a.is_constant) {
			const float c = b.constant_value;
			const float *v = a.data;
			for (uint32_t i = 0; i < buffer_size; ++i) {
				out.data[i] = f(v[i], c);
			}

		} else {
			// Normally this case should have been optimized out at compile-time
			const float c = f(a.constant_value, b.constant_value);
			for (uint32_t i = 0; i < buffer_size; ++i) {
				out.data[i] = c;
			}
		}

	} else {
		for (uint32_t i = 0; i < buffer_size; ++i) {
			out.data[i] = f(a.data[i], b.data[i]);
		}
	}
}

} // namespace zylann::voxel::pg

#endif // VOXEL_GRAPH_NODES_UTIL_H
