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

#include <cmath>
#include <array>
#include <fstream>

namespace modo {

using uint = unsigned int;
using uchar = unsigned char;
constexpr float PI = 3.1415927f;
constexpr float DT = 1.f / 44100.f;

template <class T, std::size_t N> class RingBuffer {
	T data[N];
	std::size_t start;
public:
	RingBuffer(): data(), start(0) {}
	T& operator [](std::size_t i) {
		return data[(start + i) % N];
	}
	void operator ++() {
		start = (start + 1) % N;
	}
	void operator --() {
		start = (start + (N - 1)) % N;
	}
};

template <class T, std::size_t N> class Queue {
	RingBuffer<T, N> buffer;
	std::size_t size;
public:
	Queue(): size(0) {}
	void put(const T& element) {
		buffer[size] = element;
		++size;
	}
	T take() {
		T element = buffer[0];
		++buffer;
		--size;
		return element;
	}
	bool is_empty() const {
		return size == 0;
	}
};

class Xorshift128plus {
	// xorshift128+ algorithm by Sebastiano Vigna
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
	Value(const T& value = T()): value(value) {}
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
	Input(const T& value = T()): value(value), output(&this->value) {}
	void connect(Output<T>& output) {
		this->output = &output;
	}
	void connect(const T& value) {
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
	i.connect(t);
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

template <class... T> class InputTuple;
template <class Head, class... Tail> class InputTuple<Head, Tail...> {
	Input<Head> head;
	InputTuple<Tail...> tail;
public:
	template <class Arg0, class... Arg> void connect(Arg0&& argument0, Arg&&... arguments) {
		head.connect(std::forward<Arg0>(argument0));
		tail.connect(std::forward<Arg>(arguments)...);
	}
	template <class T, class... Arg> decltype(auto) get_and_process(int t, T& node, Arg&&... arguments) {
		return tail.get_and_process(t, node, std::forward<Arg>(arguments)..., head.get(t));
	}
};
template <> class InputTuple<> {
public:
	void connect() {}
	template <class T, class... Arg> decltype(auto) get_and_process(int t, T& node, Arg&&... arguments) {
		return node.process(std::forward<Arg>(arguments)...);
	}
};

class NodeInfo {
public:
	template <class T, class Ret, class... Arg> static Ret get_return_type(Ret (T::*)(Arg...));
	template <class T, class Ret, class... Arg> static InputTuple<Arg...> get_input_tuple_type(Ret (T::*)(Arg...));
	template <class T> using return_type = decltype(get_return_type(&T::process));
	template <class T> using input_tuple_type = decltype(get_input_tuple_type(&T::process));
};

template <class T> class Node2: public T, public Output<NodeInfo::return_type<T>> {
	NodeInfo::input_tuple_type<T> inputs;
	NodeInfo::return_type<T> value;
	int t;
public:
	using T::T;
	template <class... Arg> void connect(Arg&&... arguments) {
		inputs.connect(std::forward<Arg>(arguments)...);
	}
	NodeInfo::return_type<T> get(int t) override {
		if (t != this->t) {
			this->t = t;
			value = inputs.get_and_process(t, *this);
		}
		return value;
	}
};

class Osc {
	float sin = 0.f;
	float cos = 1.f;
public:
	float process(float frequency) {
		const float f = frequency * 2.f * PI * DT;
		cos += -sin * f;
		sin += cos * f;
		return sin;
	}
};

class Saw {
	float value = 0.f;
public:
	float process(float frequency) {
		value += frequency * (2.0 * DT);
		if (value > 1.f) {
			value -= 2.f;
		}
		return value;
	}
};

class Square {
	float value = 0.f;
public:
	float process(float frequency) {
		value += frequency * DT;
		if (value > 1.f) {
			value -= 1.f;
		}
		return value > .5f ? 1.f : -1.f;
	}
};

class Noise {
public:
	float process() {
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

template <std::size_t N> class Delay: public Node<Sample> {
	RingBuffer<float, N> buffer;
public:
	Input<float> input;
	Sample produce() override {
		const float left = buffer[0] * .25;
		const float right = buffer[N/2] * .5f;
		buffer[0] = get(input) + left;
		++buffer;
		return Pan::pan(left, -.5f) + Pan::pan(right, .5f);
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

class Freeverb: public Node<Sample> {
	// freeverb algorithm by Jezar at Dreampoint
	template <std::size_t N> class Comb {
		std::array<float, N> buffer;
		std::size_t position;
		float previous;
	public:
		Comb(): buffer(), position(0), previous(0.f) {}
		float process(float input, float feedback, float damp) {
			const float output = buffer[position];
			// low-pass filter
			const float filtered = output * (1.f - damp) + previous * damp;
			previous = filtered;
			buffer[position] = input + filtered * feedback;
			position = (position + 1) % N;
			return output;
		}
	};
	template <std::size_t N> class AllPass {
		std::array<float, N> buffer;
		std::size_t position;
	public:
		AllPass(): buffer(), position(0) {}
		float process(float input) {
			constexpr float feedback = .5f;
			const float output = buffer[position];
			buffer[position] = input + output * feedback;
			position = (position + 1) % N;
			return output - input;
		}
	};
	template <std::size_t S> class Channel {
		Comb<1116+S> comb1;
		Comb<1188+S> comb2;
		Comb<1277+S> comb3;
		Comb<1356+S> comb4;
		Comb<1422+S> comb5;
		Comb<1491+S> comb6;
		Comb<1557+S> comb7;
		Comb<1617+S> comb8;
		AllPass<556+S> all_pass1;
		AllPass<441+S> all_pass2;
		AllPass<341+S> all_pass3;
		AllPass<225+S> all_pass4;
	public:
		float process(float input, float feedback, float damp) {
			float result = 0.f;
			// process comb filters in parallel
			result += comb1.process(input, feedback, damp);
			result += comb2.process(input, feedback, damp);
			result += comb3.process(input, feedback, damp);
			result += comb4.process(input, feedback, damp);
			result += comb5.process(input, feedback, damp);
			result += comb6.process(input, feedback, damp);
			result += comb7.process(input, feedback, damp);
			result += comb8.process(input, feedback, damp);
			// process all-pass filters in series
			result = all_pass1.process(result);
			result = all_pass2.process(result);
			result = all_pass3.process(result);
			result = all_pass4.process(result);
			return result;
		}
	};
	Channel<0> channel1;
	Channel<23> channel2;
public:
	Input<float> input;
	Input<float> room_size;
	Input<float> damp;
	Input<float> wet;
	Input<float> dry;
	Input<float> width;
	Sample produce() override {
		const float _input = get(input) * .03f;
		const float feedback = get(room_size) * .28f + .7f;
		const float _damp = get(damp) * .4f;
		const float output1 = channel1.process(_input, feedback, _damp);
		const float output2 = channel2.process(_input, feedback, _damp);
		const Sample w = Pan::pan(get(wet) * 3.f, get(width));
		return Sample(output1 * w.right + output2 * w.left, output2 * w.right + output1 * w.left) + get(input) * (get(dry) * 2.f);
	}
};

class Automation {
	const char* automation;
	const char* cursor;
	float value;
	float delta;
	float t;
	float parse_number() {
		float number = 0.f;
		float sign = 1.f;
		if (*cursor == '-') {
			sign = -1.f;
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
		return number * sign;
	}
	void skip_space() {
		while (*cursor == ' ') {
			++cursor;
		}
	}
public:
	Automation(const char* automation): automation(automation), cursor(automation), value(0.f), delta(0.f), t(0.f) {}
	float process() {
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

class Note {
public:
	static constexpr uchar C3  = 48;
	static constexpr uchar Db3 = 49;
	static constexpr uchar D3  = 50;
	static constexpr uchar Eb3 = 51;
	static constexpr uchar E3  = 52;
	static constexpr uchar F3  = 53;
	static constexpr uchar Gb3 = 54;
	static constexpr uchar G3  = 55;
	static constexpr uchar Ab3 = 56;
	static constexpr uchar A3  = 57;
	static constexpr uchar Bb3 = 58;
	static constexpr uchar B3  = 59;
	static constexpr uchar C4  = 60;
	static constexpr uchar Db4 = 61;
	static constexpr uchar D4  = 62;
	static constexpr uchar Eb4 = 63;
	static constexpr uchar E4  = 64;
	static constexpr uchar F4  = 65;
	static constexpr uchar Gb4 = 66;
	static constexpr uchar G4  = 67;
	static constexpr uchar Ab4 = 68;
	static constexpr uchar A4  = 69;
	static constexpr uchar Bb4 = 70;
	static constexpr uchar B4  = 71;
	static constexpr uchar C5  = 72;
	static constexpr uchar Db5 = 73;
	static constexpr uchar D5  = 74;
	static constexpr uchar Eb5 = 75;
	static constexpr uchar E5  = 76;
	static constexpr uchar F5  = 77;
	static constexpr uchar Gb5 = 78;
	static constexpr uchar G5  = 79;
	static constexpr uchar Ab5 = 80;
	static constexpr uchar A5  = 81;
	static constexpr uchar Bb5 = 82;
	static constexpr uchar B5  = 83;
};

class MIDIClock {
	float value = 1.f;
public:
	MIDIEvent process(float bpm) {
		value += bpm / 60.f * 24.f * DT;
		if (value > 1.f) {
			value -= 1.f;
			return MIDIEvent(0xF8, 0, 0);
		}
		return MIDIEvent();
	}
};

class Frequency {
	float frequency;
	float target_frequency;
	float factor = 1.f;
	uchar note = 0;
public:
	float process(MIDIEvent event) {
		if (event.is_note_on()) {
			const bool slide = note;
			note = event.data1;
			target_frequency = 440.f * std::pow(2.f, (note-69)/12.f);
			if (slide) {
				factor = std::pow(target_frequency / frequency, DT / 0.05f);
			}
			else {
				frequency = target_frequency;
				factor = 1.f;
			}
		}
		else if (event.is_note_off() && event.data1 == note) {
			note = 0;
		}
		frequency *= factor;
		if ((factor > 1.f && frequency > target_frequency) || (factor < 1.f && frequency < target_frequency)) {
			frequency = target_frequency;
			factor = 1.f;
		}
		return frequency;
	}
};

class Velocity {
	float velocity;
public:
	float process(MIDIEvent event) {
		if (event.is_note_on()) {
			velocity = event.data2 / 127.f;
		}
		return velocity;
	}
};

class NotePattern {
	uchar note;
	const char* pattern;
public:
	NotePattern(uchar note, const char* pattern): note(note), pattern(pattern) {}
	MIDIEvent operator [](std::size_t t) const {
		if (t % 6 == 5) {
			const char prev = pattern[t/6];
			const char next = pattern[(t/6+1)%16];
			if (prev != ' ' && next != '-') {
				return MIDIEvent::create_note_off(note, 127, 0);
			}
		}
		else if (t % 6 == 0) {
			const char c = pattern[t/6];
			if (c >= '0' && c <= '8') {
				return MIDIEvent::create_note_on(note, (c - '0') * 15, 0);
			}
		}
		return MIDIEvent();
	}
};

template <std::size_t N> class Pattern: public Node<MIDIEvent> {
	std::array<NotePattern, N> patterns;
	Queue<MIDIEvent, N> queue;
	int t;
public:
	Input<MIDIEvent> clock;
	Pattern(const std::array<NotePattern, N>& patterns): patterns(patterns), t(0) {}
	MIDIEvent produce() override {
		if (get(clock).status == 0xF8) {
			for (auto& pattern: patterns) {
				const MIDIEvent event = pattern[t];
				if (event) {
					queue.put(event);
				}
			}
			t = (t + 1) % (6 * 4 * 4);
		}
		if (!queue.is_empty()) {
			return queue.take();
		}
		return MIDIEvent();
	}
};

class ADSR {
	enum class State {
		Attack,
		Decay,
		Sustain,
		Release
	};
	State state = State::Sustain;
	float value = 0.f;
	uchar note = 0;
public:
	float process(MIDIEvent event, float attack, float decay, float sustain, float release) {
		if (event.is_note_on()) {
			const bool slide = note;
			note = event.data1;
			if (!slide) {
				state = State::Attack;
			}
		}
		else if (event.is_note_off() && event.data1 == note) {
			note = 0;
			state = State::Release;
		}
		switch (state) {
		case State::Attack:
			value += 1000.f / attack * DT;
			if (value >= 1.f) {
				value = 1.f;
				state = State::Decay;
			}
			break;
		case State::Decay:
			value = sustain + (value - sustain) * std::pow(0.01f, DT * 1000.f / decay);
			break;
		case State::Sustain:
			break;
		case State::Release:
			value -= 1000.f / release * DT;
			if (value <= 0.f) {
				value = 0.f;
				state = State::Sustain;
			}
			break;
		}
		return value;
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
