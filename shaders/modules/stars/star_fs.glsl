/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014                                                                    *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#version __CONTEXT__

// keep in sync with renderablestars.h:ColorOption enum
const int COLOROPTION_COLOR = 0;
const int COLOROPTION_VELOCITY = 1; 
const int COLOROPTION_SPEED = 2;

uniform sampler2D psfTexture;
uniform sampler1D colorTexture;
uniform vec3 Color;

uniform int colorOption;

layout(location = 0) in vec4 vs_position;
layout(location = 1) in vec3 ge_brightness;
layout(location = 2) in vec3 ge_velocity;
layout(location = 3) in float ge_speed;
layout(location = 4) in vec2 texCoord;



#include "ABuffer/abufferStruct.hglsl"
#include "ABuffer/abufferAddToBuffer.hglsl"
#include "PowerScaling/powerScaling_fs.hglsl"

//---------------------------------------------------------------------------

vec4 bv2rgb(float bv) {
    // BV is [-0.4,2.0]
    float t = (bv + 0.4) / (2.0 + 0.4);
    return texture(colorTexture, t);
}

void main() {
	// Something in the color calculations need to be changed because before it was dependent
	// on the gl blend functions since the abuffer was not involved

	vec4 color = vec4(0.0);
	switch (colorOption) {
		case COLOROPTION_COLOR: 
			color = bv2rgb(ge_brightness[0].x);
			break;
		case COLOROPTION_VELOCITY:
			color = vec4(abs(ge_velocity), 0.5); 
			break;
		case COLOROPTION_SPEED:
			// @TODO Include a transfer function here ---abock
			color = vec4(vec3(ge_speed), 0.5);
			break;
	}


	// color.rgb = 1/ color.rgb;
	// color.a = 1-color.a;
    framebuffer_output_color = texture(psfTexture, texCoord) * color;
    // framebuffer_output_color = vec4(1.0, 0.0, 0.0, 1.0);

    // framebuffer_output_color = vec4(ge_velocity, 1.0);

    //diffuse = vec4(1,1,0,1);
   ///diffuse = vec4(Color, 1.0);

   	vec4 position = vs_position;
	float depth = pscDepth(position);
	gl_FragDepth = depth;

	//ABufferStruct_t frag = createGeometryFragment(vec4(1,0,0,1), position, depth);
	//ABufferStruct_t frag = createGeometryFragment(diffuse, position, depth);
	//addToBuffer(frag);

	//discard;
    
}