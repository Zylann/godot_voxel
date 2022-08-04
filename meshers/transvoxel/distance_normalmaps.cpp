#include "distance_normalmaps.h"
#include "../../generators/voxel_generator.h"
#include "../../util/math/conv.h"
#include "../../util/math/triangle.h"
#include "../../util/profiling.h"

#include <core/io/image.h>
#include <scene/resources/texture.h>

namespace zylann::voxel {

static void dilate_normalmap(Span<Vector3f> normals, Vector2i size) {
	ZN_PROFILE_SCOPE();

	static const int s_dx[4] = { -1, 1, 0, 0 };
	static const int s_dy[4] = { 0, 0, -1, 1 };

	struct NewNormal {
		unsigned int loc;
		Vector3f normal;
	};
	static thread_local std::vector<NewNormal> tls_new_normals;
	tls_new_normals.clear();

	unsigned int loc = 0;
	for (int y = 0; y < size.y; ++y) {
		for (int x = 0; x < size.x; ++x) {
			if (normals[loc] != Vector3f()) {
				++loc;
				continue;
			}
			Vector3f sum;
			int count = 0;
			for (unsigned int di = 0; di < 4; ++di) {
				const int nx = x + s_dx[di];
				const int ny = y + s_dy[di];
				if (nx >= 0 && nx < size.x && ny >= 0 && ny < size.y) {
					const Vector3f nn = normals[nx + ny * size.y];
					if (nn != Vector3f()) {
						sum += nn;
						++count;
					}
				}
			}
			if (count > 0) {
				sum /= float(count);
				tls_new_normals.push_back(NewNormal{ loc, sum });
			}
			++loc;
		}
	}

	for (const NewNormal nn : tls_new_normals) {
		normals[nn.loc] = nn.normal;
	}
}

NormalMapData::Tile compute_tile_info(
		const transvoxel::CellInfo cell_info, const transvoxel::MeshArrays &mesh, unsigned int first_index) {
	Vector3f normal_sum;
	unsigned int ii = first_index;
	for (unsigned int triangle_index = 0; triangle_index < cell_info.triangle_count; ++triangle_index) {
		const unsigned vi0 = mesh.indices[ii];
		const unsigned vi1 = mesh.indices[ii + 1];
		const unsigned vi2 = mesh.indices[ii + 2];
		ii += 3;
		const Vector3f normal0 = mesh.normals[vi0];
		const Vector3f normal1 = mesh.normals[vi1];
		const Vector3f normal2 = mesh.normals[vi2];
		normal_sum += normal0;
		normal_sum += normal1;
		normal_sum += normal2;
	}
#ifdef DEBUG_ENABLED
	ZN_ASSERT(cell_info.position.x >= 0);
	ZN_ASSERT(cell_info.position.y >= 0);
	ZN_ASSERT(cell_info.position.z >= 0);
	ZN_ASSERT(cell_info.position.x < 256);
	ZN_ASSERT(cell_info.position.y < 256);
	ZN_ASSERT(cell_info.position.z < 256);
#endif
	const NormalMapData::Tile tile{ //
		uint8_t(cell_info.position.x), //
		uint8_t(cell_info.position.y), //
		uint8_t(cell_info.position.z), //
		uint8_t(math::get_longest_axis(normal_sum))
	};
	return tile;
}

void get_axis_indices(Vector3f::Axis axis, unsigned int &ax, unsigned int &ay, unsigned int &az) {
	switch (axis) {
		case Vector3f::AXIS_X:
			ax = Vector3f::AXIS_Z;
			ay = Vector3f::AXIS_Y;
			az = Vector3f::AXIS_X;
			break;
		case Vector3f::AXIS_Y:
			ax = Vector3f::AXIS_X;
			ay = Vector3f::AXIS_Z;
			az = Vector3f::AXIS_Y;
			break;
		case Vector3f::AXIS_Z:
			ax = Vector3f::AXIS_X;
			ay = Vector3f::AXIS_Y;
			az = Vector3f::AXIS_Z;
			break;
		default:
			ZN_CRASH();
	}
}

typedef FixedArray<math::BakedIntersectionTriangleForFixedDirection, transvoxel::MAX_TRIANGLES_PER_CELL> CellTriangles;

unsigned int prepare_triangles(unsigned int first_index, const transvoxel::CellInfo cell_info, const Vector3f direction,
		CellTriangles &baked_triangles, const transvoxel::MeshArrays &mesh) {
	unsigned int triangle_count = 0;

	unsigned int ii = first_index;
	for (unsigned int ti = 0; ti < cell_info.triangle_count; ++ti) {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(ii + 2 < mesh.indices.size());
#endif
		const unsigned vi0 = mesh.indices[ii];
		const unsigned vi1 = mesh.indices[ii + 1];
		const unsigned vi2 = mesh.indices[ii + 2];
		ii += 3;
		const Vector3f a = mesh.vertices[vi0];
		const Vector3f b = mesh.vertices[vi1];
		const Vector3f c = mesh.vertices[vi2];
		math::BakedIntersectionTriangleForFixedDirection baked_triangle;
		// The triangle can be parallel to the direction
		if (baked_triangle.bake(a, b, c, direction)) {
			baked_triangles[triangle_count] = baked_triangle;
			++triangle_count;
		}
	}

	return triangle_count;
}

inline uint8_t unorm_to_u8(float x) {
	return math::clamp(255.f * x, 0.f, 255.f);
}

// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
Vector2f octahedron_wrap(Vector2f v) {
	return (Vector2f(1.f) - math::abs(Vector2f(v.y, v.x))) * math::sign_nonzero(v);
}

// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
inline Vector2f encode_normal_octahedron(Vector3f n) {
	const float sum = Math::abs(n.x) + Math::abs(n.y) + Math::abs(n.z);
	n.x /= sum;
	n.y /= sum;
	Vector2f r = n.z >= 0.f ? Vector2f(n.x, n.y) : octahedron_wrap(Vector2f(n.x, n.y));
	r = r * 0.5f + Vector2f(0.5f);
	return r;
}

inline Vector3f encode_normal_xyz(const Vector3f n) {
	return Vector3f(0.5f) + 0.5f * n;
}

// For each non-empty cell of the mesh, choose an axis-aligned projection based on triangle normals in the cell.
// Sample voxels inside the cell to compute a tile of world space normals from the SDF.
void compute_normalmap(Span<const transvoxel::CellInfo> cell_infos, const transvoxel::MeshArrays &mesh,
		NormalMapData &normal_map_data, unsigned int tile_resolution, VoxelGenerator &generator,
		Vector3i origin_in_voxels, unsigned int lod_index, bool octahedral_encoding) {
	ZN_PROFILE_SCOPE();

	const unsigned int encoded_normal_size = octahedral_encoding ? 2 : 3;
	normal_map_data.normals.resize(math::squared(tile_resolution) * cell_infos.size() * encoded_normal_size);

	const unsigned int cell_size = 1 << lod_index;
	const float step = float(cell_size) / tile_resolution;

	normal_map_data.tiles.reserve(cell_infos.size());

	unsigned int first_index = 0;

	for (unsigned int cell_index = 0; cell_index < cell_infos.size(); ++cell_index) {
		const transvoxel::CellInfo cell_info = cell_infos[cell_index];

		const NormalMapData::Tile tile = compute_tile_info(cell_info, mesh, first_index);
		normal_map_data.tiles.push_back(tile);

		const Vector3f cell_origin_world = to_vec3f(origin_in_voxels + cell_info.position * cell_size);

		unsigned int ax;
		unsigned int ay;
		unsigned int az;
		get_axis_indices(Vector3f::Axis(tile.axis), ax, ay, az);

		Vector3f quad_origin_world = cell_origin_world;
		quad_origin_world[az] += cell_size * 0.5f;

		Vector3f direction;
		direction[az] = 1.f;

		static thread_local std::vector<Vector2i> tls_tile_sample_positions;
		tls_tile_sample_positions.clear();
		tls_tile_sample_positions.reserve(math::squared(tile_resolution));

		// Each normal needs 4 samples:
		// (x,   y,   z  )
		// (x+s, y,   z  )
		// (x,   y+s, z  )
		// (x,   y,   z+s)
		const unsigned int max_buffer_size = math::squared(tile_resolution) * 4;

		static thread_local std::vector<float> tls_sdf_buffer;
		static thread_local std::vector<float> tls_x_buffer;
		static thread_local std::vector<float> tls_y_buffer;
		static thread_local std::vector<float> tls_z_buffer;
		tls_sdf_buffer.clear();
		tls_x_buffer.clear();
		tls_y_buffer.clear();
		tls_z_buffer.clear();
		tls_sdf_buffer.reserve(max_buffer_size);
		tls_x_buffer.reserve(max_buffer_size);
		tls_y_buffer.reserve(max_buffer_size);
		tls_z_buffer.reserve(max_buffer_size);

		// Optimize triangles
		CellTriangles baked_triangles;
		unsigned int triangle_count = prepare_triangles(first_index, cell_info, direction, baked_triangles, mesh);

		// Fill query buffers
		{
			ZN_PROFILE_SCOPE_NAMED("Compute positions");
			for (unsigned int yi = 0; yi < tile_resolution; ++yi) {
				for (unsigned int xi = 0; xi < tile_resolution; ++xi) {
					// TODO Add bias to center differences when calculating the normals?
					Vector3f pos000 = quad_origin_world;
					// Casting to `int` here because even if the target is float, temporaries can be negative uints
					pos000[ax] += int(xi) * step;
					pos000[ay] += int(yi) * step;

					// Project to triangles
					const Vector3f ray_origin_world = pos000 - direction * cell_size;
					const Vector3f ray_origin_mesh = ray_origin_world - to_vec3f(origin_in_voxels);
					const float NO_HIT = 999999.f;
					float nearest_hit_distance = NO_HIT;
					for (unsigned int ti = 0; ti < triangle_count; ++ti) {
						const math::TriangleIntersectionResult result =
								baked_triangles[ti].intersect(ray_origin_mesh, direction);
						if (result.case_id == math::TriangleIntersectionResult::INTERSECTION &&
								result.distance < nearest_hit_distance) {
							nearest_hit_distance = result.distance;
						}
					}

					if (nearest_hit_distance == NO_HIT) {
						// Don't query if there is no triangle
						continue;
					}

					pos000 = ray_origin_world + direction * nearest_hit_distance;
					tls_tile_sample_positions.push_back(Vector2i(xi, yi));

					tls_x_buffer.push_back(pos000.x);
					tls_y_buffer.push_back(pos000.y);
					tls_z_buffer.push_back(pos000.z);

					Vector3f pos100 = pos000;
					pos100.x += step;
					tls_x_buffer.push_back(pos100.x);
					tls_y_buffer.push_back(pos100.y);
					tls_z_buffer.push_back(pos100.z);

					Vector3f pos010 = pos000;
					pos010.y += step;
					tls_x_buffer.push_back(pos010.x);
					tls_y_buffer.push_back(pos010.y);
					tls_z_buffer.push_back(pos010.z);

					Vector3f pos001 = pos000;
					pos001.z += step;
					tls_x_buffer.push_back(pos001.x);
					tls_y_buffer.push_back(pos001.y);
					tls_z_buffer.push_back(pos001.z);
				}
			}
		}

		tls_sdf_buffer.resize(tls_x_buffer.size());

		// Query voxel data
		// TODO Support edited voxels
		// TODO Support modifiers
		generator.generate_series(to_span(tls_x_buffer), to_span(tls_y_buffer), to_span(tls_z_buffer),
				VoxelBufferInternal::CHANNEL_SDF, to_span(tls_sdf_buffer), cell_origin_world,
				cell_origin_world + Vector3f(cell_size));

		static thread_local std::vector<Vector3f> tls_tile_normals;
		tls_tile_normals.clear();
		tls_tile_normals.resize(math::squared(tile_resolution));

		// Compute normals from SDF results
		{
			ZN_PROFILE_SCOPE_NAMED("Compute normals");
			unsigned int bi = 0;
			for (const Vector2i sample_position : tls_tile_sample_positions) {
				const unsigned int bi000 = bi;
				const unsigned int bi100 = bi + 1;
				const unsigned int bi010 = bi + 2;
				const unsigned int bi001 = bi + 3;
				bi += 4;
				// TODO I wish this was solved https://github.com/godotengine/godot/issues/31608
#ifdef DEBUG_ENABLED
				ZN_ASSERT(bi000 < tls_sdf_buffer.size());
				ZN_ASSERT(bi100 < tls_sdf_buffer.size());
				ZN_ASSERT(bi010 < tls_sdf_buffer.size());
				ZN_ASSERT(bi001 < tls_sdf_buffer.size());
#endif
				const float sd000 = tls_sdf_buffer[bi000];
				const float sd100 = tls_sdf_buffer[bi100];
				const float sd010 = tls_sdf_buffer[bi010];
				const float sd001 = tls_sdf_buffer[bi001];
				const Vector3f normal = math::normalized(Vector3f(sd100 - sd000, sd010 - sd000, sd001 - sd000));
				const unsigned int normal_index = sample_position.x + sample_position.y * tile_resolution;
#ifdef DEBUG_ENABLED
				ZN_ASSERT(normal_index < normal_map_data.normals.size());
#endif
				tls_tile_normals[normal_index] = normal;
			}
		}

		for (unsigned int dilation_steps = 0; dilation_steps < 2; ++dilation_steps) {
			// Fill up some pixels around triangle borders, to give some margin when sampling near them in shader
			dilate_normalmap(to_span(tls_tile_normals), Vector2i(tile_resolution, tile_resolution));
		}

		// Encode normals
		const unsigned int tile_begin = cell_index * math::squared(tile_resolution) * encoded_normal_size;
		if (octahedral_encoding) {
			for (unsigned int i = 0; i < tls_tile_normals.size(); ++i) {
				const unsigned int offset = tile_begin + i * encoded_normal_size;
				ZN_ASSERT(offset + encoded_normal_size <= normal_map_data.normals.size());
				const Vector2f n = encode_normal_octahedron(tls_tile_normals[i]);
				normal_map_data.normals[offset + 0] = unorm_to_u8(n.x);
				normal_map_data.normals[offset + 1] = unorm_to_u8(n.y);
			}
		} else {
			for (unsigned int i = 0; i < tls_tile_normals.size(); ++i) {
				const unsigned int offset = tile_begin + i * encoded_normal_size; //
				ZN_ASSERT(offset + encoded_normal_size <= normal_map_data.normals.size());
				const Vector3f n = encode_normal_xyz(tls_tile_normals[i]);
				normal_map_data.normals[offset + 0] = unorm_to_u8(n.x);
				normal_map_data.normals[offset + 1] = unorm_to_u8(n.y);
				normal_map_data.normals[offset + 2] = unorm_to_u8(n.z);
			}
		}

		first_index += 3 * cell_info.triangle_count;
	}
}

NormalMapImages store_normalmap_data_to_images(
		const NormalMapData &data, unsigned int tile_resolution, Vector3i block_size, bool octahedral_encoding) {
	ZN_PROFILE_SCOPE();

	NormalMapImages images;

	{
		ZN_PROFILE_SCOPE_NAMED("Atlas images");
		Vector<Ref<Image>> tile_images;
		tile_images.resize(data.tiles.size());

		{
			const unsigned int pixel_size = octahedral_encoding ? 2 : 3;
			const Image::Format format = octahedral_encoding ? Image::FORMAT_RG8 : Image::FORMAT_RGB8;

			for (unsigned int tile_index = 0; tile_index < data.tiles.size(); ++tile_index) {
				PackedByteArray bytes;
				{
					const unsigned int tile_size_in_pixels = math::squared(tile_resolution);
					const unsigned int tile_size_in_bytes = tile_size_in_pixels * pixel_size;
					bytes.resize(tile_size_in_bytes);
					memcpy(bytes.ptrw(), data.normals.data() + tile_index * tile_size_in_bytes, tile_size_in_bytes);
				}

				Ref<Image> image;
				image.instantiate();
				image->create_from_data(tile_resolution, tile_resolution, false, format, bytes);

				tile_images.write[tile_index] = image;
				//image->save_png(String("debug_atlas_{0}.png").format(varray(tile_index)));
			}
		}

		images.atlas_images = tile_images;
	}

	{
		ZN_PROFILE_SCOPE_NAMED("Lookup image+texture");

		const unsigned int sqri = Math::ceil(Math::sqrt(double(Vector3iUtil::get_volume(block_size))));

		PackedByteArray bytes;
		{
			const unsigned int pixel_size = 3;
			bytes.resize(math::squared(sqri) * pixel_size);

			uint8_t *bytes_w = bytes.ptrw();
			memset(bytes_w, 0, bytes.size());

			const unsigned int deck_size = block_size.x * block_size.y;

			for (unsigned int tile_index = 0; tile_index < data.tiles.size(); ++tile_index) {
				const NormalMapData::Tile tile = data.tiles[tile_index];
				// RG: layer index
				// B: projection direction
				const uint8_t r = tile_index & 0xff;
				const uint8_t g = tile_index >> 8;
				const uint8_t b = tile.axis;
				const unsigned int pi = pixel_size * (tile.x + tile.y * block_size.x + tile.z * deck_size);
				ZN_ASSERT(pi < bytes.size());
				bytes_w[pi] = r;
				bytes_w[pi + 1] = g;
				bytes_w[pi + 2] = b;
			}
		}

		Ref<Image> image;
		image.instantiate();
		image->create_from_data(sqri, sqri, false, Image::FORMAT_RGB8, bytes);
		images.lookup_image = image;
	}

	return images;
}

// Converts normalmap data into textures. They can be used in a shader to apply normals and obtain extra visual details.
NormalMapTextures store_normalmap_data_to_textures(const NormalMapImages &data) {
	ZN_PROFILE_SCOPE();

	NormalMapTextures textures;

	{
		ZN_PROFILE_SCOPE_NAMED("Atlas texture");
		Ref<Texture2DArray> atlas;
		atlas.instantiate();
		const Error err = atlas->create_from_images(data.atlas_images);
		ZN_ASSERT_RETURN_V(err == OK, textures);
		textures.atlas = atlas;
	}

	{
		ZN_PROFILE_SCOPE_NAMED("Lookup image+texture");
		Ref<ImageTexture> lookup = ImageTexture::create_from_image(data.lookup_image);
		textures.lookup = lookup;
	}

	return textures;
}

} // namespace zylann::voxel
