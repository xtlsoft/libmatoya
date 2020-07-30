// Copyright (c) 2020 Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifdef GL_ES
	precision mediump float;
#endif

varying vec2 v_texcoord;

uniform sampler2D sample0;
uniform sampler2D sample1;
uniform sampler2D sample2;

uniform float scale;
uniform int format;

void main(void) {
	vec2 proj = vec2(
		v_texcoord[0] * scale,
		v_texcoord[1]
	);

	// NV12
	if (format == 2) {
		float y = texture2D(sample0, proj).r;
		float u = texture2D(sample1, proj).r;
		float v = texture2D(sample1, proj).g;

		y = (y - 0.0625) * 1.164;
		u = u - 0.5;
		v = v - 0.5;

		float r = y + 1.793 * v;
		float g = y - 0.213 * u - 0.533 * v;
		float b = y + 2.112 * u;

		gl_FragColor = vec4(r, g, b, 1.0);

	// I420 and I444
	} else if (format == 3 || format == 4) {
		float y = texture2D(sample0, proj).r;
		float u = texture2D(sample1, proj).r;
		float v = texture2D(sample2, proj).r;

		y = (y - 0.0625) * 1.164;
		u = u - 0.5;
		v = v - 0.5;

		float r = y + 1.793 * v;
		float g = y - 0.213 * u - 0.533 * v;
		float b = y + 2.112 * u;

		gl_FragColor = vec4(r, g, b, 1.0);

	// RGBA
	} else {
		gl_FragColor = texture2D(sample0, proj);
	}
}
