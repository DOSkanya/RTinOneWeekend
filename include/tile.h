#pragma once
#include "rtweekend.h"
#include "bvh.h"
#include "camera.h"
#include "material.h"
#include <thread>
#include <mutex>

extern std::mutex change;

color ray_color(const ray& r, const color& background, const bvh_node& bvh, int depth) {
	hit_record rec;

	//If we've exceeded the ray bounce limit, no more light is gathered.
	if (depth <= 0)
		return color(0, 0, 0);

	if (!bvh.hit(r, 0.001, infinity, rec))
		return background;

	ray scattered;
	color attenuation;
	color emitted = rec.mat_ptr->emitted(rec.u, rec.v, rec.p);

	if (!rec.mat_ptr->scatter(r, rec, attenuation, scattered))
		return emitted;

	return emitted + attenuation * ray_color(scattered, background, bvh, depth - 1);
}

class tile {
public:
	tile(int w_b, int w_e, int h_b, int h_e, int i_w, int i_h) {
		width_begin = w_b;
		width_end = w_e;
		height_begin = h_b;
		height_end = h_e;
		image_width = i_w;
		image_height = i_h;
		color_block = new color[(width_end - width_begin) * (height_end - height_begin)];
	}

	void render(bvh_node& bvh, camera& cam, color& background, int max_depth, int samples_per_pixel);
public:
	static int cores_left;
	int image_width;
	int image_height;
	int width_begin;
	int width_end;
	int height_begin;
	int height_end;
	color* color_block;
};

int tile::cores_left;

void thread_function(tile& t, bvh_node& bvh, camera& cam, color& background, int max_depth, int samples_per_pixel) {
	for (int j = t.height_end - 1; j >= t.height_begin; --j) {
		//std::cerr << "\rScanlines remaining: " << j << ' ' << std::flush;
		for (int i = t.width_begin; i < t.width_end; ++i) {
			color pixel_color(0, 0, 0);
			for (int s = 0; s < samples_per_pixel; ++s) {
				auto u = (i + random_double()) / (t.image_width - 1);
				auto v = (j + random_double()) / (t.image_height - 1);
				ray r = cam.get_ray(u, v);
				pixel_color += ray_color(r, background, bvh, max_depth);
			}
			t.color_block[(j - t.height_begin) * (t.width_end - t.width_begin) + (i - t.width_begin)] = pixel_color;
		}
	}
	change.lock();
	t.cores_left = t.cores_left + 1;
	change.unlock();

	std::cerr << "thread has quit.\n";
}

void tile::render(bvh_node& bvh, camera& cam, color& background, int max_depth, int samples_per_pixel) {
	std::cerr << "tile has been created." << std::endl;
	change.lock();
	cores_left = cores_left - 1;
	std::thread rendering(thread_function, std::ref(*this), std::ref(bvh), std::ref(cam), std::ref(background), max_depth, samples_per_pixel);
	rendering.detach();
	change.unlock();
}