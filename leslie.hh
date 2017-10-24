/*

Copyright (c) 2017, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#pragma once

#include "modo.hh"

namespace modo {

class Leslie: public Node<Sample> {
	RingBuffer<float, 32> buffer;
	float sin;
	float cos;
	float get_linear(float i) {
		size_t lower = i;
		float factor = i - lower;
		size_t upper = lower + 1;
		return buffer[lower] * (1.f - factor) + buffer[upper] * factor;
	}
public:
	Input<float> input;
	Input<float> frequency_factor;
	Leslie(): sin(0.f), cos(1.f), frequency_factor(1.f) {}
	Sample produce() override {
		--buffer;
		buffer[0] = get(input);
		const float f = .0002f * get(frequency_factor);
		cos += -sin * f;
		sin += cos * f;
		return Pan::pan(get_linear(sin * 15.f + 16.f), cos * .3f) + Pan::pan(get_linear(sin * -15.f + 16.f), cos * -.3f);
	}
};

} // namespace modo