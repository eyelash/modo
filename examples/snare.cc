#include "../modo.hh"

using namespace modo;

class Snare {
	class Head {
		Osc osc;
		Automation frequency = "4000 4000/.001 400/.002 200/.01";
		Automation envelope = "0 1.3/.0002 .15/.05 0/.05";
	public:
		float process() {
			float result = osc.process(frequency.process());
			return Clip::process(result * envelope.process());
		}
	};
	class Tail {
		Noise noise;
		Resonator resonator;
		Automation envelope = "0 .9/.03 .05/.05 0/.1";
	public:
		float process() {
			float result = 0.f;
			result += resonator.process(noise.process(), .5f, .3f) * .6f;
			result += noise.process() * .4f;
			return result * envelope.process();
		}
	};
	Head head;
	Tail tail;
public:
	float process() {
		float sample = 0.f;
		sample += head.process();
		sample += tail.process();
		return sample;
	}
};

int main() {
	Node2<Snare> snare;
	Node2<Pan> pan;
	pan.connect(snare, 0.f);
	WAVOutput output("snare.wav");
	output.run(pan, 44100);
}
