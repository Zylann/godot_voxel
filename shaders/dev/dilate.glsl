#[compute]
#version 450

// Dilates a normalmap by filling "empty" pixels with the average of surrounding pixels.
// Assumes the input image is tiled: dilation will not interact across tiles.
// One run of this shader will dilate by 1 pixel.

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout (set = 0, binding = 0, rgba8ui) restrict readonly uniform uimage2D u_src_image;
layout (set = 0, binding = 1, rgba8ui) restrict writeonly uniform uimage2D u_dst_image;
layout (set = 0, binding = 2) uniform Params {
	int u_tile_size;
};

void main() {
	// This color corresponds to a null normal.
	const ivec4 nocol = ivec4(127, 127, 127, 255);
	const ivec2 pixel_pos = ivec2(gl_GlobalInvocationID.xy);

	const ivec4 col11 = ivec4(imageLoad(u_src_image, pixel_pos));
	if (col11 != nocol) {
		imageStore(u_dst_image, pixel_pos, col11);
		return;
	}

	//const ivec2 im_size = imageSize(u_src_image).xy;

	const ivec2 p01 = pixel_pos + ivec2(-1, 0);
	const ivec2 p21 = pixel_pos + ivec2(1, 0);
	const ivec2 p10 = pixel_pos + ivec2(0, -1);
	const ivec2 p12 = pixel_pos + ivec2(0, 1);

	ivec4 col_sum = ivec4(0,0,0,0);
	int count = 0;

	const ivec4 col01 = ivec4(imageLoad(u_src_image, p01));
	// Don't sample pixels of different tiles than the current one.
	// This also takes care of image borders, but we must do it more explicitely for negative borders
	// because of how division works
	if (col01 != nocol && pixel_pos.x != 0 && (pixel_pos.x - 1) / u_tile_size == pixel_pos.x / u_tile_size) {
		col_sum += col01;
		++count;
	}

	const ivec4 col21 = ivec4(imageLoad(u_src_image, p21));
	if (col21 != nocol && (pixel_pos.x + 1) / u_tile_size == pixel_pos.x / u_tile_size) {
		col_sum += col21;
		++count;
	}

	const ivec4 col10 = ivec4(imageLoad(u_src_image, p10));
	if (col10 != nocol && pixel_pos.y != 0 && (pixel_pos.y - 1) / u_tile_size == pixel_pos.y / u_tile_size) {
		col_sum += col10;
		++count;
	}

	const ivec4 col12 = ivec4(imageLoad(u_src_image, p12));
	if (col12 != nocol && (pixel_pos.y + 1) / u_tile_size == pixel_pos.y / u_tile_size) {
		col_sum += col12;
		++count;
	}

	ivec4 col_avg = count == 0 ? col11 : col_sum / count;

	imageStore(u_dst_image, pixel_pos, col_avg);
}
