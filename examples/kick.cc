#include "../modo.hh"
using namespace modo;

class Kick: public Node<float> {
	Osc osc;
	Automation frequency;
	Automation envelope;
public:
	Kick(): frequency("130 45/.1"), envelope("0 .9/.01 .3/.2 0/.4") {
		frequency >> osc.frequency;
	}
	float produce() override {
		return osc.get() * envelope.get();
	}
};

int main() {
	Kick kick;
	WAVOutput wav("kick.wav");
	kick >> wav.input;
	wav.run(44100);
}
