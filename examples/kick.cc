#include "../modo.hh"
using namespace modo;

class Kick: public Node<Sample> {
	Osc osc;
	Automation frequency;
	Automation envelope;
public:
	Kick(): frequency("130 45/.1"), envelope("0 .9/.01 .3/.2 0/.4") {
		frequency >> osc.frequency;
	}
	Sample produce() override {
		return get(osc) * get(envelope);
	}
};

class Kick2: public Node<Sample> {
	Osc osc;
	Automation frequency;
	Automation envelope;
public:
	Kick2(): frequency("3000 3000/.0005 500/.002 150/.01 50/.1"), envelope("0 .8/.0002 .8/.2 0/.1") {
		frequency >> osc.frequency;
	}
	Sample produce() override {
		return get(osc) * get(envelope);
	}
};

int main() {
	Kick kick;
	WAVOutput wav("kick.wav");
	kick >> wav.input;
	wav.run(44100);
}
