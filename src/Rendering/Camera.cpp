#include "Camera.h"

#include <iostream>
#include <fstream>
#include <random>

#include "../Geometry/Ray.h"
#include "../Utility/Math.h"

Camera::Camera(const int _width, const int _height) : width(_width), height(_height) {
	pixels.assign(width, std::vector<Pixel>(height));
	discretizedPixels.assign(width, std::vector<glm::u8vec3>(height));
}

void Camera::Render(const Scene & scene, const glm::vec3 eye,
					const glm::vec3 c1, const glm::vec3 c2,
					const glm::vec3 c3, const glm::vec3 c4,
					const float RAY_LENGTH, const unsigned int RAYS_PER_PIXEL) {

	std::cout << "Rendering the scene ..." << std::endl;

	std::random_device rd;
	std::uniform_real_distribution<float> rand(0, 1.0f);
	std::default_random_engine r(rd());

	float invWidth = 1.0f / (float)width;
	float invHeight = 1.0f / (float)height;

	/* For each pixel, shoot a bunch of rays through it. */
	unsigned int y, z;
	for (unsigned int y = 0; y < width; ++y) {
		for (unsigned int z = 0; z < height; ++z) {

			/* Shoot a bunch of rays through the pixel (y, z). */
			glm::vec3 colorAccumulator = glm::vec3(0, 0, 0);
			for (unsigned int i = 0; i < RAYS_PER_PIXEL; ++i) {

				/* Calculate new randomized point the camera plane. */
				float ylerp = (y + rand(r)) * invWidth;
				float zlerp = (z + rand(r)) * invHeight;
				float nx = Math::InterpolateQuad4f(ylerp, zlerp, c1.x, c2.x, c3.x, c4.x);
				float ny = Math::InterpolateQuad4f(ylerp, zlerp, c1.y, c2.y, c3.y, c4.y);
				float nz = Math::InterpolateQuad4f(ylerp, zlerp, c1.z, c2.z, c3.z, c4.z);

				/* Create ray. */
				const glm::vec3 planePosition(nx, ny, nz); // The camera plane intersection position.
				const glm::vec3 rayDirection = glm::normalize(planePosition - eye);
				const Ray ray(eye, planePosition, planePosition + RAY_LENGTH * rayDirection);

				/* Trace ray through the scene. */
				colorAccumulator += scene.TraceRay(ray);
			}

			/* Set pixel color dependent on ray trace. */
			float f = 1.0f / static_cast<float>(RAYS_PER_PIXEL);
			pixels[y][z].color = f * colorAccumulator;
		}
	}

	/* Create the final discretized image from float the values. */
	CreateImage(); // Should always be done immediately after the rendering step.
}

void Camera::CreateImage() {
	std::cout << "Creating a discretized image from the rendered image ..." << std::endl;

	/* Find max color intensity. Could try using r + g + b instead. */
	float maxIntensity = 0;
	for (size_t i = 0; i < width; ++i) {
		for (size_t j = 0; j < height; ++j) {
			maxIntensity = glm::max(maxIntensity, pixels[i][j].color.r);
			maxIntensity = glm::max(maxIntensity, pixels[i][j].color.g);
			maxIntensity = glm::max(maxIntensity, pixels[i][j].color.b);
		}
	}

	// TODO: Change this to some other way of detecting whether the image is dark (with spots).
	if (maxIntensity > BRIGHTNESS_DISCRETIZATION_THRESHOLD) {
		for (size_t i = 0; i < width; ++i) {
			for (size_t j = 0; j < height; ++j) {
				pixels[i][j].color.r = sqrt(pixels[i][j].color.r);
				pixels[i][j].color.g = sqrt(pixels[i][j].color.g);
				pixels[i][j].color.b = sqrt(pixels[i][j].color.b);
			}
		}
	}

	// Discretize pixels using the max intensity. Every value must be between 0 and 255.
	float f = maxIntensity < 0.00001f ? 0.0f : 254.99f / maxIntensity;
	for (size_t i = 0; i < width; ++i) {
		for (size_t j = 0; j < height; ++j) {
			float r = pixels[i][j].color.r * f;
			assert(r >= -FLT_EPSILON && r <= 256 - FLT_EPSILON);
			float g = pixels[i][j].color.g * f;
			assert(g >= -FLT_EPSILON && g <= 256 - FLT_EPSILON);
			float b = pixels[i][j].color.b * f;
			assert(b >= -FLT_EPSILON && b <= 256 - FLT_EPSILON);
			discretizedPixels[i][j].r = (glm::u8)round(r);
			discretizedPixels[i][j].g = (glm::u8)round(g);
			discretizedPixels[i][j].b = (glm::u8)round(b);
		}
	}
}

bool Camera::WriteImageToTGA(const std::string path) const {
	std::cout << "Writing image to TGA ..." << std::endl;

	assert(width > 0 && height > 0);

	std::ofstream o(path.c_str(), std::ios::out | std::ios::binary);

	// Write header.
	const std::string header = "002000000000";
	for (unsigned int i = 0; i < header.length(); ++i) {
		o.put(header[i] - '0');
	}

	o.put(width & 0x00FF);
	o.put((width & 0xFF00) >> 8);
	o.put(width & 0x00FF);
	o.put((height & 0xFF00) >> 8);
	o.put(32); // 24 bit bitmap.
	o.put(0);

	// Write data.
	for (unsigned int y = 0; y < height; ++y) {
		for (unsigned int x = 0; x < width; ++x) {
			auto& cp = discretizedPixels[x][y];
			o.put(cp.b);
			o.put(cp.g);
			o.put(cp.r);
			o.put((char)0xFF); // 255.
		}
	}

	o.close();

	return true;
}