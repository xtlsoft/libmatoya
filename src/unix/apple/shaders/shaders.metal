// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include <metal_stdlib>

using namespace metal;

typedef struct {
	packed_float2 position;
	packed_float2 texcoord;
} Vertex;

typedef struct {
	float4 position [[position]];
	float2 texcoord;
} VSOut;

vertex VSOut vs(
	const device Vertex *verticies [[buffer(0)]],
	unsigned int vid [[vertex_id]]
) {
	const device Vertex &v = verticies[vid];

	VSOut out;
	out.position = float4(float2(v.position), 0.0, 1.0);
	out.texcoord = v.texcoord;

	return out;
}

fragment float4 fs(
	VSOut in [[stage_in]],
	texture2d<float, access::sample> tex0 [[texture(0)]],
	texture2d<float, access::sample> tex1 [[texture(1)]],
	texture2d<float, access::sample> tex2 [[texture(2)]],
	constant uint &format [[buffer(0)]],
	sampler s [[sampler(0)]]
) {
	//NV12

	if (format == 2) {
		float y = tex0.sample(s, in.texcoord).r;
		float u = tex1.sample(s, in.texcoord).r;
		float v = tex1.sample(s, in.texcoord).g;

		y = (y - 0.0625) * 1.164;
		u = u - 0.5;
		v = v - 0.5;

		float r = y + 1.793 * v;
		float g = y - 0.213 * u - 0.533 * v;
		float b = y + 2.112 * u;

		return float4(r, g, b, 1.0);

	// I420 or I444
	} else if (format == 3 || format == 4) {
		float y = tex0.sample(s, in.texcoord).r;
		float u = tex1.sample(s, in.texcoord).r;
		float v = tex2.sample(s, in.texcoord).r;

		y = (y - 0.0625) * 1.164;
		u = u - 0.5;
		v = v - 0.5;

		float r = y + 1.793 * v;
		float g = y - 0.213 * u - 0.533 * v;
		float b = y + 2.112 * u;

		return float4(r, g, b, 1.0);
	}

	// RGBA
	return tex0.sample(s, in.texcoord);
}
