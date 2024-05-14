#ifndef YUBINOBUTAI_BASICDATA_H
#define YUBINOBUTAI_BASICDATA_H

#include <cstdint>

union Vector2 {
	struct {
		float x, y;
	};
	struct {
		float u, v;
	};
	float index[2];
};

union Vector3 {
	struct {
		float x, y, z;
	};
	struct {
		float r, g, b;
	};
	float index[3];
};

union Vector4 {
	struct {
		float x, y, z, w;
	};
	struct {
		float r, g, b, a;
	};
	float index[4];
};

struct Vertex {
	Vector3 position;
	Vector2 uv;
};

using Index = std::uint16_t;

#endif // YUBINOBUTAI_BASICDATA_H