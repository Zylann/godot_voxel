#include "counting.h"
#include "../util/containers/std_unordered_map.h"
#include "materials_4i4w.h"

namespace zylann::voxel {

uint32_t count_sdf_lower_than_value(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId channel_index,
		const float isolevel
) {
	uint32_t count = 0;

	switch (voxels.get_channel_compression(channel_index)) {
		case VoxelBuffer::COMPRESSION_UNIFORM: {
			const float sd = voxels.get_voxel_f(Vector3i(), channel_index);
			if (sd < isolevel) {
				count = Vector3iUtil::get_volume_u64(voxels.get_size());
			}
		} break;

		case VoxelBuffer::COMPRESSION_NONE: {
			const VoxelBuffer::Depth depth = voxels.get_channel_depth(channel_index);

			switch (depth) {
				case VoxelBuffer::DEPTH_8_BIT: {
					Span<const int8_t> channel_data;
					ZN_ASSERT_RETURN_V(voxels.get_channel_data_read_only(channel_index, channel_data), 0);
					const int8_t isolevel_i8 = snorm_to_s8(isolevel * constants::QUANTIZED_SDF_8_BITS_SCALE_INV);
					for (const int8_t v : channel_data) {
						count += static_cast<uint32_t>(v < isolevel_i8);
					}
				} break;

				case VoxelBuffer::DEPTH_16_BIT: {
					Span<const int16_t> channel_data;
					ZN_ASSERT_RETURN_V(voxels.get_channel_data_read_only(channel_index, channel_data), 0);
					const int16_t isolevel_i16 = snorm_to_s16(isolevel * constants::QUANTIZED_SDF_16_BITS_SCALE_INV);
					for (const int16_t v : channel_data) {
						count += static_cast<uint32_t>(v < isolevel_i16);
					}
				} break;

				case VoxelBuffer::DEPTH_32_BIT: {
					Span<const float> channel_data;
					ZN_ASSERT_RETURN_V(voxels.get_channel_data_read_only(channel_index, channel_data), 0);
					for (const float v : channel_data) {
						count += static_cast<uint32_t>(v < isolevel);
					}
				} break;

				default:
					ZN_PRINT_ERROR("Unhandled depth");
					break;
			}
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled compression");
			break;
	}

	return count;
}

std::array<uint64_t, 16> count_materials_mixel4(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId indices_channel_index,
		const VoxelBuffer::ChannelId weights_channel_index
) {
	std::array<uint64_t, 16> raw_counts = {};

	ZN_ASSERT_RETURN_V(voxels.get_channel_depth(indices_channel_index) == VoxelBuffer::DEPTH_16_BIT, raw_counts);
	ZN_ASSERT_RETURN_V(voxels.get_channel_depth(weights_channel_index) == VoxelBuffer::DEPTH_16_BIT, raw_counts);

	switch (voxels.get_channel_compression(indices_channel_index)) {
		case VoxelBuffer::COMPRESSION_UNIFORM: {
			const uint16_t encoded_indices = voxels.get_voxel(Vector3i(), indices_channel_index);
			const FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(encoded_indices);

			switch (voxels.get_channel_compression(weights_channel_index)) {
				case VoxelBuffer::COMPRESSION_UNIFORM: {
					const uint64_t volume = Vector3iUtil::get_volume_u64(voxels.get_size());

					const uint16_t encoded_weights = voxels.get_voxel(Vector3i(), weights_channel_index);
					const FixedArray<uint8_t, 4> weights = decode_weights_from_packed_u16(encoded_weights);

					for (unsigned int i = 0; i < 4; ++i) {
						const uint8_t mi = indices[i];
						const uint64_t w = weights[i];
						raw_counts[mi] = w * volume;
					}
				} break;

				case VoxelBuffer::COMPRESSION_NONE: {
					Span<const uint16_t> encoded_weights_s;
					ZN_ASSERT(voxels.get_channel_data_read_only(weights_channel_index, encoded_weights_s));

					for (const uint16_t encoded_weights : encoded_weights_s) {
						const FixedArray<uint8_t, 4> weights = decode_weights_from_packed_u16(encoded_weights);

						for (unsigned int i = 0; i < weights.size(); ++i) {
							const uint8_t mi = indices[i];
							const uint64_t w = weights[i];
							raw_counts[mi] += w;
						}
					}
				} break;

				default:
					ZN_PRINT_ERROR("Unhandled compression");
					break;
			}
		} break;

		case VoxelBuffer::COMPRESSION_NONE: {
			switch (voxels.get_channel_compression(weights_channel_index)) {
				case VoxelBuffer::COMPRESSION_UNIFORM: {
					const uint16_t encoded_weights = voxels.get_voxel(Vector3i(), weights_channel_index);
					const FixedArray<uint8_t, 4> weights = decode_weights_from_packed_u16(encoded_weights);

					Span<const uint16_t> encoded_indices_s;
					ZN_ASSERT(voxels.get_channel_data_read_only(indices_channel_index, encoded_indices_s));

					for (const uint16_t encoded_indices : encoded_indices_s) {
						const FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(encoded_indices);

						for (unsigned int i = 0; i < weights.size(); ++i) {
							const uint8_t mi = indices[i];
							const uint64_t w = weights[i];
							raw_counts[mi] += w;
						}
					}
				} break;

				case VoxelBuffer::COMPRESSION_NONE: {
					Span<const uint16_t> encoded_indices_s;
					Span<const uint16_t> encoded_weights_s;
					ZN_ASSERT(voxels.get_channel_data_read_only(indices_channel_index, encoded_indices_s));
					ZN_ASSERT(voxels.get_channel_data_read_only(indices_channel_index, encoded_weights_s));

					for (unsigned int voxel_index = 0; voxel_index < encoded_indices_s.size(); ++voxel_index) {
						const uint16_t encoded_indices = encoded_indices_s[voxel_index];
						const uint16_t encoded_weights = encoded_weights_s[voxel_index];

						const FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(encoded_indices);
						const FixedArray<uint8_t, 4> weights = decode_weights_from_packed_u16(encoded_weights);

						for (unsigned int i = 0; i < weights.size(); ++i) {
							const uint8_t mi = indices[i];
							const uint64_t w = weights[i];
							raw_counts[mi] += w;
						}
					}
				} break;

				default:
					ZN_PRINT_ERROR("Unhandled compression");
					break;
			}
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled compression");
			break;
	}

	return raw_counts;
}

std::array<uint64_t, 16> count_materials_mixel4_with_sdf_lower_than_value(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId sdf_channel_index,
		const VoxelBuffer::ChannelId indices_channel_index,
		const VoxelBuffer::ChannelId weights_channel_index,
		const float isolevel
) {
	std::array<uint64_t, 16> raw_counts = {};

	switch (voxels.get_channel_compression(sdf_channel_index)) {
		case VoxelBuffer::COMPRESSION_UNIFORM: {
			const float sd = voxels.get_voxel_f(Vector3i(), sdf_channel_index);
			if (sd < isolevel) {
				raw_counts = count_materials_mixel4(voxels, indices_channel_index, weights_channel_index);
			}
		} break;

		case VoxelBuffer::COMPRESSION_NONE: {
			// Less optimized for now. Otherwise there are quite a lot of combinations.

			ZN_ASSERT_RETURN_V(
					voxels.get_channel_depth(indices_channel_index) == VoxelBuffer::DEPTH_16_BIT, raw_counts
			);
			ZN_ASSERT_RETURN_V(
					voxels.get_channel_depth(weights_channel_index) == VoxelBuffer::DEPTH_16_BIT, raw_counts
			);

			Vector3i pos;
			for (pos.z = 0; pos.z < voxels.get_size().z; ++pos.z) {
				for (pos.x = 0; pos.x < voxels.get_size().x; ++pos.x) {
					for (pos.y = 0; pos.y < voxels.get_size().y; ++pos.y) {
						const float sd = voxels.get_voxel_f(pos, sdf_channel_index);
						if (sd >= isolevel) {
							continue;
						}
						const uint16_t encoded_indices = voxels.get_voxel(pos, indices_channel_index);
						const uint16_t encoded_weights = voxels.get_voxel(pos, weights_channel_index);

						const FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(encoded_indices);
						const FixedArray<uint8_t, 4> weights = decode_weights_from_packed_u16(encoded_weights);

						for (unsigned int i = 0; i < weights.size(); ++i) {
							const uint8_t mi = indices[i];
							const uint64_t w = weights[i];
							raw_counts[mi] += w;
						}
					}
				}
			}

		} break;

		default:
			ZN_PRINT_ERROR("Unhandled compression");
			break;
	}

	return raw_counts;
}

template <typename F>
uint32_t count_comparisons_with_value(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId channel_index,
		const uint32_t cmp_value,
		F f
) {
	uint32_t count = 0;

	switch (voxels.get_channel_compression(channel_index)) {
		case VoxelBuffer::COMPRESSION_UNIFORM: {
			const uint32_t v = voxels.get_voxel(Vector3i(), channel_index);
			if (f(v, cmp_value)) {
				count = Vector3iUtil::get_volume_u64(voxels.get_size());
			}
		} break;

		case VoxelBuffer::COMPRESSION_NONE: {
			switch (voxels.get_channel_depth(channel_index)) {
				case VoxelBuffer::DEPTH_8_BIT: {
					Span<const uint8_t> channel_data;
					ZN_ASSERT_RETURN_V(voxels.get_channel_data_read_only(channel_index, channel_data), 0);
					for (const uint8_t v : channel_data) {
						count += static_cast<uint32_t>(f(v, cmp_value));
					}
				} break;

				case VoxelBuffer::DEPTH_16_BIT: {
					Span<const uint16_t> channel_data;
					ZN_ASSERT_RETURN_V(voxels.get_channel_data_read_only(channel_index, channel_data), 0);
					for (const uint16_t v : channel_data) {
						count += static_cast<uint32_t>(f(v, cmp_value));
					}
				} break;

				default:
					ZN_PRINT_ERROR("Unhandled depth");
					break;
			}
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled compression");
			break;
	}

	return count;
}

template <typename F>
uint32_t count_comparisons_with_buffer(
		const VoxelBuffer &src_voxels,
		const VoxelBuffer::ChannelId channel_index,
		const VoxelBuffer &cmp_voxels,
		F f
) {
	ZN_ASSERT_RETURN_V(src_voxels.get_size() == cmp_voxels.get_size(), 0);

	const VoxelBuffer::Depth src_depth = src_voxels.get_channel_depth(channel_index);
	const VoxelBuffer::Depth cmp_depth = cmp_voxels.get_channel_depth(channel_index);
	ZN_ASSERT_RETURN_V(src_depth == cmp_depth, 0);

	const VoxelBuffer::Compression src_compression = src_voxels.get_channel_compression(channel_index);
	const VoxelBuffer::Compression cmp_compression = cmp_voxels.get_channel_compression(channel_index);

	uint32_t count = 0;

	switch (src_compression) {
		case VoxelBuffer::COMPRESSION_UNIFORM: {
			switch (cmp_compression) {
				case VoxelBuffer::COMPRESSION_UNIFORM: {
					const uint32_t src_value = src_voxels.get_voxel(Vector3i(), channel_index);
					const uint32_t cmp_value = cmp_voxels.get_voxel(Vector3i(), channel_index);
					if (f(src_value, cmp_value)) {
						count = Vector3iUtil::get_volume_u64(src_voxels.get_size());
					}
				} break;

				case VoxelBuffer::COMPRESSION_NONE: {
					const uint8_t src_value = src_voxels.get_voxel(Vector3i(), channel_index);

					switch (src_depth) {
						case VoxelBuffer::DEPTH_8_BIT: {
							Span<const uint8_t> cmp_channel_data;
							ZN_ASSERT_RETURN_V(
									cmp_voxels.get_channel_data_read_only(channel_index, cmp_channel_data), 0
							);
							for (const uint8_t cmp_value : cmp_channel_data) {
								count += static_cast<uint32_t>(f(src_value, cmp_value));
							}
						} break;

						case VoxelBuffer::DEPTH_16_BIT: {
							Span<const uint16_t> cmp_channel_data;
							ZN_ASSERT_RETURN_V(
									cmp_voxels.get_channel_data_read_only(channel_index, cmp_channel_data), 0
							);
							for (const uint16_t cmp_value : cmp_channel_data) {
								count += static_cast<uint32_t>(f(src_value, cmp_value));
							}
						} break;

						default:
							ZN_PRINT_ERROR("Unhandled depth");
							break;
					}
				} break;

				default:
					ZN_PRINT_ERROR("Unhandled cmp compression");
					break;
			}
		} break;

		case VoxelBuffer::COMPRESSION_NONE: {
			switch (cmp_compression) {
				case VoxelBuffer::COMPRESSION_UNIFORM: {
					const uint8_t cmp_value = cmp_voxels.get_voxel(Vector3i(), channel_index);

					switch (src_depth) {
						case VoxelBuffer::DEPTH_8_BIT: {
							Span<const uint8_t> src_channel_data;
							ZN_ASSERT_RETURN_V(
									src_voxels.get_channel_data_read_only(channel_index, src_channel_data), 0
							);
							for (const uint8_t src_value : src_channel_data) {
								count += static_cast<uint32_t>(f(src_value, cmp_value));
							}
						} break;

						case VoxelBuffer::DEPTH_16_BIT: {
							Span<const uint16_t> src_channel_data;
							ZN_ASSERT_RETURN_V(
									cmp_voxels.get_channel_data_read_only(channel_index, src_channel_data), 0
							);
							for (const uint16_t src_value : src_channel_data) {
								count += static_cast<uint32_t>(f(src_value, cmp_value));
							}
						} break;

						default:
							ZN_PRINT_ERROR("Unhandled depth");
							break;
					}
				} break;

				case VoxelBuffer::COMPRESSION_NONE: {
					switch (src_depth) {
						case VoxelBuffer::DEPTH_8_BIT: {
							Span<const uint8_t> src_channel_data;
							Span<const uint8_t> cmp_channel_data;
							ZN_ASSERT_RETURN_V(
									src_voxels.get_channel_data_read_only(channel_index, src_channel_data), 0
							);
							ZN_ASSERT_RETURN_V(
									cmp_voxels.get_channel_data_read_only(channel_index, cmp_channel_data), 0
							);
							for (unsigned int i = 0; i < src_channel_data.size(); ++i) {
								count += static_cast<uint32_t>(f(src_channel_data[i], cmp_channel_data[i]));
							}
						} break;

						case VoxelBuffer::DEPTH_16_BIT: {
							Span<const uint16_t> src_channel_data;
							Span<const uint16_t> cmp_channel_data;
							ZN_ASSERT_RETURN_V(
									src_voxels.get_channel_data_read_only(channel_index, src_channel_data), 0
							);
							ZN_ASSERT_RETURN_V(
									cmp_voxels.get_channel_data_read_only(channel_index, cmp_channel_data), 0
							);
							for (unsigned int i = 0; i < src_channel_data.size(); ++i) {
								count += static_cast<uint32_t>(f(src_channel_data[i], cmp_channel_data[i]));
							}
						} break;

						default:
							ZN_PRINT_ERROR("Unhandled depth");
							break;
					}
				} break;

				default:
					ZN_PRINT_ERROR("Unhandled cmp compression");
					break;
			}
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled src compression");
			break;
	}

	return count;
}

uint32_t count_not_equal_to_buffer(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId channel,
		const VoxelBuffer &other
) {
	return count_comparisons_with_buffer(voxels, channel, other, [](uint32_t a, uint32_t b) { return a != b; });
}

uint32_t count_equal_to_value(const VoxelBuffer &voxels, const VoxelBuffer::ChannelId channel, const uint32_t value) {
	return count_comparisons_with_value(voxels, channel, value, [](uint32_t a, uint32_t b) { return a == b; });
}

uint32_t count_not_equal_to_value(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId channel,
		const uint32_t value
) {
	const uint64_t volume = Vector3iUtil::get_volume_u64(voxels.get_size());
	return volume - count_equal_to_value(voxels, channel, value);
}

template <typename T>
void count_values_uncompressed(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId channel_index,
		StdUnorderedMap<uint32_t, uint32_t> &counts
) {
#ifdef DEV_ENABLED
	ZN_ASSERT(voxels.get_channel_compression(channel_index) == VoxelBuffer::COMPRESSION_NONE);
#endif

	Span<const T> channel_data;
	ZN_ASSERT_RETURN(voxels.get_channel_data_read_only(channel_index, channel_data));

	// Minimize lookups by only doing so when the value changes.
	// To initialize this algorithm we need to treat the first value separately.
	const T first_v = channel_data[0];
	StdUnorderedMap<uint32_t, uint32_t>::iterator it = counts.insert({ first_v, 1 }).first;
	uint32_t prev_v = first_v;

	channel_data = channel_data.sub(1);

	for (const T v : channel_data) {
		if (v != prev_v) {
			it = counts.find(v);
			if (it == counts.end()) {
				it = counts.insert({ v, 0 }).first;
			}
			prev_v = v;
		}
		it->second += 1;
	}
}

StdUnorderedMap<uint32_t, uint32_t> count_values(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId channel_index
) {
	StdUnorderedMap<uint32_t, uint32_t> counts;

	switch (voxels.get_channel_compression(channel_index)) {
		case VoxelBuffer::COMPRESSION_UNIFORM: {
			const uint32_t v = voxels.get_voxel(Vector3i(), channel_index);
			const uint32_t volume = Vector3iUtil::get_volume_u64(voxels.get_size());
			counts.insert({ v, volume });
		} break;

		case VoxelBuffer::COMPRESSION_NONE: {
			switch (voxels.get_channel_depth(channel_index)) {
				case VoxelBuffer::DEPTH_8_BIT: {
					count_values_uncompressed<uint8_t>(voxels, channel_index, counts);
				} break;

				case VoxelBuffer::DEPTH_16_BIT: {
					count_values_uncompressed<uint16_t>(voxels, channel_index, counts);
				} break;

				default: {
					ZN_PRINT_ERROR("Unhandled depth");
				} break;
			}
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled compression");
			break;
	}

	return counts;
}

template <typename TRawSD>
void count_values_u8_with_uncompressed_sdf_lower_than(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId sd_channel_index,
		const VoxelBuffer::ChannelId indices_channel_index,
		const TRawSD raw_isolevel,
		std::array<uint32_t, 256> &counts
) {
#ifdef DEV_ENABLED
	ZN_ASSERT(voxels.get_channel_compression(sd_channel_index) == VoxelBuffer::COMPRESSION_NONE);
#endif

	switch (voxels.get_channel_compression(indices_channel_index)) {
		case VoxelBuffer::COMPRESSION_UNIFORM: {
			const uint32_t vi = voxels.get_voxel(Vector3i(), indices_channel_index);
			Span<const TRawSD> sd_data;
			ZN_ASSERT_RETURN(voxels.get_channel_data_read_only(sd_channel_index, sd_data));
			uint32_t count = 0;
			for (const TRawSD v : sd_data) {
				count += static_cast<uint32_t>(v < raw_isolevel);
			}
			counts[vi] = count;
		} break;

		case VoxelBuffer::COMPRESSION_NONE: {
			Span<const uint8_t> indices_data;
			Span<const TRawSD> sd_data;
			ZN_ASSERT_RETURN(voxels.get_channel_data_read_only(sd_channel_index, sd_data));
			ZN_ASSERT_RETURN(voxels.get_channel_data_read_only(indices_channel_index, indices_data));
			for (unsigned int i = 0; i < sd_data.size(); ++i) {
				const TRawSD sd = sd_data[i];
				const uint8_t vi = indices_data[i];
				if (sd < raw_isolevel) {
					counts[i] += 1;
				}
			}
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled compression");
			break;
	}
}

void count_values_u8_with_sdf_lower_than(
		const VoxelBuffer &voxels,
		const VoxelBuffer::ChannelId sd_channel_index,
		const VoxelBuffer::ChannelId indices_channel_index,
		const float isolevel,
		std::array<uint32_t, 256> &counts
) {
	ZN_ASSERT_RETURN(voxels.get_channel_depth(indices_channel_index) == VoxelBuffer::DEPTH_8_BIT);

	std::fill(counts.begin(), counts.end(), 0);

	switch (voxels.get_channel_compression(sd_channel_index)) {
		case VoxelBuffer::COMPRESSION_UNIFORM: {
			const float sd = voxels.get_voxel_f(Vector3i(), sd_channel_index);

			if (sd < isolevel) {
				switch (voxels.get_channel_compression(indices_channel_index)) {
					case VoxelBuffer::COMPRESSION_UNIFORM: {
						const uint32_t i = voxels.get_voxel(Vector3i(), indices_channel_index);
						counts[i] = Vector3iUtil::get_volume_u64(voxels.get_size());
					} break;

					case VoxelBuffer::COMPRESSION_NONE: {
						Span<const uint8_t> channel_data;
						ZN_ASSERT_RETURN(voxels.get_channel_data_read_only(indices_channel_index, channel_data));
						for (const uint8_t v : channel_data) {
							counts[v] += 1;
						}
					} break;

					default:
						ZN_PRINT_ERROR("Unhandled compression");
						break;
				}
			}
		} break;

		case VoxelBuffer::COMPRESSION_NONE: {
			const VoxelBuffer::Depth sd_depth = voxels.get_channel_depth(sd_channel_index);

			switch (sd_depth) {
				case VoxelBuffer::DEPTH_8_BIT: {
					const int8_t isolevel_i8 = snorm_to_s8(isolevel * constants::QUANTIZED_SDF_8_BITS_SCALE_INV);
					count_values_u8_with_uncompressed_sdf_lower_than<int8_t>(
							voxels, sd_channel_index, indices_channel_index, isolevel_i8, counts
					);
				} break;

				case VoxelBuffer::DEPTH_16_BIT: {
					const int16_t isolevel_i16 = snorm_to_s16(isolevel * constants::QUANTIZED_SDF_16_BITS_SCALE_INV);
					count_values_u8_with_uncompressed_sdf_lower_than<int16_t>(
							voxels, sd_channel_index, indices_channel_index, isolevel_i16, counts
					);
				} break;

				case VoxelBuffer::DEPTH_32_BIT: {
					count_values_u8_with_uncompressed_sdf_lower_than<float>(
							voxels, sd_channel_index, indices_channel_index, isolevel, counts
					);
				} break;

				default:
					ZN_PRINT_ERROR("Unhandled depth");
					break;
			}
		} break;

		default:
			ZN_PRINT_ERROR("Unhandled compression");
			break;
	}
}

} // namespace zylann::voxel
