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

class Xorshift128plus {
	uint64_t s[2];
public:
	Xorshift128plus(): s{0, 0xC0DEC0DEC0DEC0DE} {}
	uint64_t get_next() {
		const uint64_t result = s[0] + s[1];
		const uint64_t s1 = s[0] ^ (s[0] << 23);
		s[0] = s[1];
		s[1] = s1 ^ s[1] ^ (s1 >> 18) ^ (s[1] >> 5);
		return result;
	}
};

class Random {
public:
	static uint64_t get() {
		static Xorshift128plus generator;
		return generator.get_next();
	}
	static float get_float() {
		return get() / static_cast<float>(0xFFFFFFFFFFFFFFFF);
	}
};

struct Sample {
	float left;
	float right;
	constexpr Sample(): left(0.f), right(0.f) {}
	constexpr Sample(float sample): left(sample), right(sample) {}
	constexpr Sample(float left, float right): left(left), right(right) {}
	constexpr Sample operator +(const Sample& sample) const {
		return Sample(left + sample.left, right + sample.right);
	}
	constexpr Sample operator *(float f) const {
		return Sample(left * f, right * f);
	}
};

template <class T> class Output {
public:
	virtual T get(int t) = 0;
};

template <class T> class Value: public Output<T> {
	T value;
public:
	Value(T value = T()): value(value) {}
	void set(const T& value) {
		this->value = value;
	}
	T get(int t) override {
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
	T get(int t) override {
		return output->get(t);
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
	int t;
public:
	Node(): value(), t(0) {}
	virtual T produce() = 0;
	template <class T2> T2 get(Output<T2>& output) const {
		return output.get(t);
	}
	T get(int t) override {
		if (t != this->t) {
			this->t = t;
			value = produce();
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
		const float f = get(frequency) * 2.f * PI * DT;
		cos += -sin * f;
		sin += cos * f;
		return sin;
	}
};

class Saw: public Node<float> {
	float value;
public:
	Input<float> frequency;
	Saw(): value(0.f) {}
	float produce() override {
		value += get(frequency) * 2.f * DT;
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
		value += get(frequency) * DT;
		if (value > 1.f) {
			value = 0.f;
		}
		return value > .5f ? 1.f : -1.f;
	}
};

class Noise: public Node<float> {
public:
	float produce() override {
		return Random::get_float() * 2.f - 1.f;
	}
};

class Gain: public Node<float> {
public:
	Input<float> input;
	Input<float> amount;
	float produce() override {
		return get(input) * get(amount);
	}
};

class Pan: public Node<Sample> {
public:
	static constexpr Sample pan(float input, float panning) {
		return Sample(input * (.5f - panning * .5f), input * (.5f + panning * .5f));
	}
	Input<float> input;
	Input<float> panning;
	Sample produce() override {
		return pan(get(input), get(panning));
	}
};

class Mono: public Node<float> {
public:
	Input<Sample> input;
	float produce() override {
		const Sample sample = get(input);
		return (sample.left + sample.right) * .5f;
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
		return clamp(get(input) * get(amount));
	}
};

template <size_t N> class Delay: public Node<float> {
	float buffer[N];
	size_t position;
public:
	Input<float> input;
	Delay(): buffer(), position(0) {}
	float produce() override {
		const float sample = buffer[position];
		buffer[position] = get(input);
		position = (position + 1) % N;
		return sample;
	}
};

class Resonator: public Node<float> {
	float y;
	float v;
public:
	Input<float> input;
	Input<float> frequency;
	Input<float> sensitivity;
	Resonator(): y(0.f), v(0.f) {}
	float produce() override {
		const float f = get(frequency) * 2.f * PI;
		const float s = get(sensitivity);
		const float F = (get(input)*s - y)*f*f - v*s*f;
		v += F * DT;
		y += v * DT;
		return y;
	}
};

class Automation: public Node<float> {
	const char* automation;
	const char* cursor;
	float value;
	float delta;
	float t;
	float parse_number() {
		float number = 0.f;
		bool negative = false;
		if (*cursor == '-') {
			negative = true;
			++cursor;
		}
		while (*cursor >= '0' && *cursor <= '9') {
			number = (number * 10.f) + (*cursor - '0');
			++cursor;
		}
		if (*cursor == '.') {
			++cursor;
			float factor = .1f;
			while (*cursor >= '0' && *cursor <= '9') {
				number += (*cursor - '0') * factor;
				factor /= 10.f;
				++cursor;
			}
		}
		return number;
	}
	void skip_space() {
		while (*cursor == ' ') {
			++cursor;
		}
	}
public:
	Automation(const char* automation): automation(automation), cursor(automation), value(0.f), delta(0.f), t(0.f) {}
	float produce() override {
		value += delta * DT;
		t -= DT;
		if (t <= 0.f) {
			if (*cursor != '\0') {
				const float new_value = parse_number();
				if (*cursor == '/') {
					++cursor;
					t = parse_number();
					delta = (new_value - value) / t;
				}
				else {
					value = new_value;
					delta = 0.f;
				}
				skip_space();
			}
			else {
				delta = 0.f;
			}
		}
		return value;
	}
};

struct MIDIEvent {
	uchar status;
	uchar data1;
	uchar data2;
	constexpr MIDIEvent(): status(0), data1(0), data2(2) {}
	constexpr MIDIEvent(uchar status, uchar data1, uchar data2): status(status), data1(data1), data2(data2) {}
	static constexpr MIDIEvent create_note_off(uchar note, uchar velocity, uchar channel) {
		return MIDIEvent(0x80 | channel, note, velocity);
	}
	static constexpr MIDIEvent create_note_on(uchar note, uchar velocity, uchar channel) {
		return MIDIEvent(0x90 | channel, note, velocity);
	}
	constexpr operator bool() const {
		return status & 0x80;
	}
	constexpr bool is_note_off() const {
		return (status & 0xF0) == 0x80;
	}
	constexpr bool is_note_on() const {
		return (status & 0xF0) == 0x90;
	}
	constexpr uchar get_channel() const {
		return status & 0x0F;
	}
};

class MIDIClock: public Node<MIDIEvent> {
	float value;
public:
	Input<float> bpm;
	MIDIClock(): value(1.f) {}
	MIDIEvent produce() override {
		value += get(bpm) / 60.f * 24.f * DT;
		if (value > 1.f) {
			value -= 1.f;
			return MIDIEvent(0xF8, 0, 0);
		}
		return MIDIEvent();
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
	Input<Sample> input;
	WAVOutput(const char* file_name): file(file_name, std::ios_base::binary) {}
	void run(int frames) {
		write_tag("RIFF");
		write<uint32_t>(36 + frames * 2 * 2);
		write_tag("WAVE");

		write_tag("fmt ");
		write<uint32_t>(16); // fmt chunk size
		write<uint16_t>(1); // format
		write<uint16_t>(2); // channels
		write<uint32_t>(44100); // sample rate
		write<uint32_t>(44100 * 2 * 2); // bytes per second
		write<uint16_t>(2 * 2); // bytes per frame
		write<uint16_t>(16); // bits per sample

		write_tag("data");
		write<uint32_t>(frames * 2 * 2);
		for (int t = 1; t <= frames; ++t) {
			const Sample sample = input.get(t);
			write<int16_t>(sample.left * 32767.f + .5f);
			write<int16_t>(sample.right * 32767.f + .5f);
		}
	}
};

} // namespace modo

#endif // MODO_HH
