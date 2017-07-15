/*

Copyright (c) 2017, Elias Aebi
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef MODO_HH
#define MODO_HH

#include <cmath>
#include <array>
#include <fstream>

namespace modo {

using uint = unsigned int;
using uchar = unsigned char;
constexpr float PI = 3.1415927f;
constexpr float DT = 1.f / 44100.f;

class Time {
	uint t;
public:
	constexpr Time(): t(0) {}
	static Time& now() {
		static Time time;
		return time;
	}
	constexpr bool operator <(const Time& rhs) const {
		return t < rhs.t;
	}
	void operator ++() {
		++t;
	}
};

template <class T> class Output {
public:
	virtual T get() = 0;
};

template <class T> class Value: public Output<T> {
	T value;
public:
	Value(T value = T()): value(value) {}
	void set(const T& value) {
		this->value = value;
	}
	T get() override {
		return value;
	}
};

template <class T> class Input: public Output<T> {
	Value<T> value;
	Output<T>* output;
public:
	Input(): output(&value) {}
	void connect(Output<T>& output) {
		this->output = &output;
	}
	void set(const T& value) {
		this->value.set(value);
		this->output = &this->value;
	}
	T get() override {
		return output->get();
	}
};

template <class T> void operator >>(Output<T>& o, Input<T>& i) {
	i.connect(o);
}
template <class T> void operator >>(const T& t, Input<T>& i) {
	i.set(t);
}

template <class T> class Node: public Output<T> {
	T value;
	Time time;
public:
	virtual T produce() = 0;
	T get() override {
		while (time < Time::now()) {
			value = produce();
			++time;
		}
		return value;
	}
};

class Osc: public Node<float> {
	float sin;
	float cos;
public:
	Input<float> frequency;
	Osc(): sin(0.f), cos(1.f) {}
	float produce() override {
		cos += -sin * frequency.get() * 2.f * PI * DT;
		sin += cos * frequency.get() * 2.f * PI * DT;
		return sin;
	}
};

class Saw: public Node<float> {
	float value;
public:
	Input<float> frequency;
	Saw(): value(0.f) {}
	float produce() override {
		value += frequency.get() * 2.f * DT;
		if (value > 1.f) {
			value = -1.f;
		}
		return value;
	}
};

class Square: public Node<float> {
	float value;
public:
	Input<float> frequency;
	Square(): value(0.f) {}
	float produce() override {
		value += frequency.get() * DT;
		if (value > 1.f) {
			value = 0.f;
		}
		return value > .5f ? 1.f : -1.f;
	}
};

class Gain: public Node<float> {
public:
	Input<float> input;
	Input<float> amount;
	float produce() override {
		const float _amount = amount.get();
		if (_amount > 0.f) {
			return input.get() * _amount;
		}
		return 0.f;
	}
};

class Overdrive: public Node<float> {
	static constexpr float clamp(float y) {
		return y > 1.f ? 1.f : (y < -1.f ? -1.f : y);
	}
public:
	Input<float> input;
	Input<float> amount;
	float produce() override {
		return clamp(input.get() * amount.get());
	}
};

class WAVOutput {
	std::ofstream file;
	template <class T> void write(T data) {
		file.write(reinterpret_cast<const char*>(&data), sizeof(T));
	}
	void write_tag(const char* tag) {
		file.write(tag, 4);
	}
public:
	Input<float> input;
	WAVOutput(const char* file_name): file(file_name, std::ios_base::binary) {}
	void run(size_t frames) {
		write_tag("RIFF");
		write<uint32_t>(36 + frames * 2);
		write_tag("WAVE");

		write_tag("fmt ");
		write<uint32_t>(16); // fmt chunk size
		write<uint16_t>(1); // format
		write<uint16_t>(1); // channels
		write<uint32_t>(44100); // sample rate
		write<uint32_t>(44100 * 2); // bytes per second
		write<uint16_t>(2); // bytes per frame
		write<uint16_t>(16); // bits per sample

		write_tag("data");
		write<uint32_t>(frames * 2);
		for (size_t i = 0; i < frames; ++i) {
			write<int16_t>(input.get() * 32767.f + .5f);
			++Time::now();
		}
	}
};

} // namespace modo

#endif // MODO_HH
