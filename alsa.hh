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
#include <alsa/asoundlib.h>

namespace modo {

class ALSAOutput {
	static void play_buffer(snd_pcm_t* pcm, int16_t* buffer, int frames) {
		while (frames > 0) {
			int frames_written = snd_pcm_writei(pcm, buffer, frames);
			if (frames_written < 0) {
				fprintf(stderr, "ALSAOutput error: %s\n", snd_strerror(frames_written));
				snd_pcm_recover(pcm, frames_written, 0);
				snd_pcm_prepare(pcm);
				continue;
			}
			buffer += 2 * frames_written;
			frames -= frames_written;
		}
	}
public:
	void run(Output<Sample>& input) {
		constexpr int BUFFER_SIZE = 1024;
		snd_pcm_t* pcm;
		snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
		snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, 44100, 1, 20000);
		snd_pcm_prepare(pcm);
		int16_t buffer[BUFFER_SIZE * 2];
		while (true) {
			for (int i = 0; i < BUFFER_SIZE; ++i) {
				const Sample sample = input.get(i);
				buffer[i*2] = sample.left * 32767.f + .5f;
				buffer[i*2+1] = sample.right * 32767.f + .5f;
			}
			play_buffer(pcm, buffer, BUFFER_SIZE);
		}
		snd_pcm_drain(pcm);
		snd_pcm_close(pcm);
	}
};

class ALSAInput {
	snd_seq_t* seq;
	int this_client;
	int this_port;
	void subscribe (uchar dest_client, uchar dest_port, uchar sender_client, uchar sender_port) {
		snd_seq_addr_t dest = {dest_client, dest_port};
		snd_seq_addr_t sender = {sender_client, sender_port};
		snd_seq_port_subscribe_t* subsciption;
		snd_seq_port_subscribe_alloca(&subsciption);
		snd_seq_port_subscribe_set_dest(subsciption, &dest);
		snd_seq_port_subscribe_set_sender(subsciption, &sender);
		snd_seq_subscribe_port(seq, subsciption);
	}
	void connect() {
		snd_seq_client_info_t* client_info;
		snd_seq_client_info_alloca(&client_info);
		snd_seq_port_info_t* port_info;
		snd_seq_port_info_alloca(&port_info);

		snd_seq_client_info_set_client(client_info, -1);
		while (snd_seq_query_next_client(seq, client_info) >= 0) {
			const int client = snd_seq_client_info_get_client(client_info);
			snd_seq_port_info_set_client(port_info, client);
			snd_seq_port_info_set_port(port_info, -1);
			while (snd_seq_query_next_port(seq, port_info) == 0) {
				const int port = snd_seq_port_info_get_port(port_info);
				const unsigned int capability = snd_seq_port_info_get_capability(port_info);
				if ((capability & SND_SEQ_PORT_CAP_READ) && (capability & SND_SEQ_PORT_CAP_SUBS_READ) && !(capability & SND_SEQ_PORT_CAP_NO_EXPORT)) {
					printf("ALSAInput: subscribing to %s - %s (%d:%d)\n", snd_seq_client_info_get_name(client_info), snd_seq_port_info_get_name(port_info), client, port);
					subscribe(this_client, this_port, client, port);
				}
			}
		}
	}
public:
	ALSAInput() {
		snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
		this_client = snd_seq_client_id(seq);
		this_port = snd_seq_create_simple_port(seq, "MIDI input", SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_APPLICATION);
		connect();
	}
	MIDIEvent process() {
		snd_seq_event_t* event;
		if (snd_seq_event_input(seq, &event) >= 0) {
			if (event->type == SND_SEQ_EVENT_NOTEON) {
				return MIDIEvent::create_note_on(event->data.note.note, event->data.note.velocity, event->data.note.channel);
			} else if (event->type == SND_SEQ_EVENT_NOTEOFF) {
				return MIDIEvent::create_note_off(event->data.note.note, event->data.note.velocity, event->data.note.channel);
			} else if (event->type == SND_SEQ_EVENT_CONTROLLER) {
				return MIDIEvent(0xB0 | event->data.control.channel, event->data.control.param, event->data.control.value);
			}
		}
		return MIDIEvent();
	}
};

} // namespace modo
