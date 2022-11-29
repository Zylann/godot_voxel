#[compute]
#version 450

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout (set = 0, binding = 0, rgba8ui) restrict readonly uniform uimage2D u_src_image;
layout (set = 0, binding = 1, rgba8ui) restrict writeonly uniform uimage2D u_dst_image;

void main() {
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

	// In the rare cases where we read outside the image,
	// the returned color is all zeros. We also ignore black parts of the image.
	const ivec4 col01 = ivec4(imageLoad(u_src_image, p01));
	if (col01 != nocol && col01.xyz != ivec3(0,0,0)) {
		col_sum += col01;
		++count;
	}

	const ivec4 col21 = ivec4(imageLoad(u_src_image, p21));
	if (col21 != nocol && col21.xyz != ivec3(0,0,0)) {
		col_sum += col21;
		++count;
	}

	const ivec4 col10 = ivec4(imageLoad(u_src_image, p10));
	if (col10 != nocol && col10.xyz != ivec3(0,0,0)) {
		col_sum += col10;
		++count;
	}

	const ivec4 col12 = ivec4(imageLoad(u_src_image, p12));
	if (col12 != nocol && col12.xyz != ivec3(0,0,0)) {
		col_sum += col12;
		++count;
	}

	ivec4 col_avg = count == 0 ? col11 : col_sum / count;

	imageStore(u_dst_image, pixel_pos, col_avg);
}
