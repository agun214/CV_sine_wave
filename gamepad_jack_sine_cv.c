#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <jack/jack.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <jack/ringbuffer.h>
#include <alsa/asoundlib.h>
/*

gcc -o gamepad_jack_sine_cv gamepad_jack_sine_cv.c -ljack -lm -lasound

*/

jack_port_t *output_port_1;
jack_port_t *output_port_2;
jack_client_t *client;

//double amplitude = 1.0;
double amplitude = 0.5;
double frequency = 440.0;
double phase = 0.0;

//double amplitude2 = 1.0;
double amplitude2 = 0.5;
double frequency2 = 440.0;
double phase2 = 0.0;


int process(jack_nframes_t nframes, void *arg) {
    jack_default_audio_sample_t *out_1;
    jack_default_audio_sample_t *out_2;

    out_1 = jack_port_get_buffer(output_port_1, nframes);
    out_2 = jack_port_get_buffer(output_port_2, nframes);

    float sample_rate = jack_get_sample_rate(client);
    size_t buffer_size = nframes * sizeof(jack_default_audio_sample_t);

    for (int i = 0; i < nframes; i++) {
        phase += frequency / sample_rate * 2 * M_PI;
        if (phase > 2 * M_PI) {
            phase -= 2 * M_PI;
        }
        *out_1++ = amplitude * sin(phase);

        phase2 += frequency2 / sample_rate * 2 * M_PI;
		if (phase2 > 2 * M_PI) {
            phase2 -= 2 * M_PI;
        }
        *out_2++ = amplitude2 * sin(phase2);
    }

    return 0;
}

//	process_audio(buffer_size, client, output_port_1, output_ringbuffer, amplitude, frequency, phase);
int process_audio(jack_nframes_t nframes, void *arg, jack_port_t *output_port, jack_ringbuffer_t *output_ringbuffer, double amplitude, double frequency, double phase) {
    jack_default_audio_sample_t *out;
    out = jack_port_get_buffer(output_port, nframes);

    float sample_rate = jack_get_sample_rate(client);
    size_t buffer_size = nframes * sizeof(jack_default_audio_sample_t);

    for (int i = 0; i < nframes; i++) {
        phase += frequency / sample_rate * 2 * M_PI;
        if (phase > 2 * M_PI) {
            phase -= 2 * M_PI;
        }
        *out++ = amplitude * sin(phase);
    }

	//out = jack_port_get_buffer(output_port, nframes);
	jack_ringbuffer_read(output_ringbuffer, (void *)out, buffer_size * sizeof(float));
    return 0;
}

int main(int argc, char **argv) {
    // open joystick device
    int joystick_fd = open("/dev/input/js0", O_RDONLY);

    // check if joystick device was opened successfully
    if (joystick_fd == -1) {
        printf("Error: Failed to open joystick device.\n");
        return 1;
    }

	printf("Device found. joystick_fd: %d\n", joystick_fd);

    // initialize joystick input
    struct js_event js;

    jack_nframes_t buffer_size;
    float *audio_buffer_1;
    float *audio_buffer_2;

    // Obtain the buffer size from JACK
    buffer_size = jack_get_buffer_size(client);

    // Allocate memory for the buffer
    audio_buffer_1 = malloc(buffer_size * sizeof(float));
    audio_buffer_2 = malloc(buffer_size * sizeof(float));

    if (!audio_buffer_1) {
        printf("Failed to allocate memory for audio buffer 1.\n");
        exit(1);
    }
    if (!audio_buffer_2) {
        printf("Failed to allocate memory for audio buffer 2.\n");
        exit(1);
    }

    jack_ringbuffer_t *output_ringbuffer_1;
    jack_ringbuffer_t *output_ringbuffer_2;
    output_ringbuffer_1 = jack_ringbuffer_create(buffer_size * sizeof(float));
    output_ringbuffer_2 = jack_ringbuffer_create(buffer_size * sizeof(float));

    jack_status_t status;
    client = jack_client_open("Sine wave", JackNullOption, &status);

    if (client == NULL) {
        printf("jack_client_open() failed, status = 0x%2.0x\n", status);
        if (status & JackServerFailed) {
            printf("Unable to connect to JACK server\n");
        }
        exit(1);
    }

    jack_set_process_callback(client, process, client);

    output_port_1 = jack_port_register(client, "output_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	output_port_2 = jack_port_register(client, "output_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 1);

    if (jack_activate(client)) {
        printf("Cannot activate client");
        exit(1);
    }
    // Open a MIDI device
    snd_seq_t* midi = NULL;
    snd_seq_open(&midi, "default", SND_SEQ_OPEN_OUTPUT, 0);
    snd_seq_set_client_name(midi, "Gamepad MIDI");

    // Create a MIDI port
    int port = snd_seq_create_simple_port(midi, "Gamepad", SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ, SND_SEQ_PORT_TYPE_MIDI_GENERIC);


	while (1) {
		/* Read joystick events */
		if (read(joystick_fd, &js, sizeof(js)) == -1) {
		    /* Error reading joystick */
		    break;
		}

		/* Check if it is an axis event */
		if (js.type == JS_EVENT_AXIS) {
			switch(js.number){
				case 0:
					frequency = ((js.value + 32767) / 3276.7);
					printf("js.value: %d, frequency: %f\n", js.value, frequency);
					process_audio(buffer_size, client, output_port_1, output_ringbuffer_1, amplitude, frequency, phase);
					break;

				case 1:
					//amplitude2 = (js.value / -40000.0);
					amplitude2 = (js.value / -32767.0);
					printf("js.value: %d, amplitude2: %f\n", js.value, amplitude2);
					process_audio(buffer_size, client, output_port_2, output_ringbuffer_2, amplitude2, frequency2, phase2);
					break;

				case 3:
					// works for CUTOFF FREQUENCY CV: frequency 0-20Hz
					frequency2 = ((js.value + 32767) / 3276.7);
				    //frequency2 = ((js.value + 32767) / 3.2767);
					printf("js.value: %d, frequency2: %f\n", js.value, frequency2);
					process_audio(buffer_size, client, output_port_2, output_ringbuffer_2, amplitude2, frequency2, phase2);
					break;

				case 4:
					//amplitude = (js.value / -40000.0);
					amplitude = (js.value / -32767.0);
					printf("js.value: %d, amplitude: %f\n", js.value, amplitude);
					process_audio(buffer_size, client, output_port_1, output_ringbuffer_1, amplitude, frequency, phase);
					break;

				default:
					break;
			}
		}

		if (js.type == JS_EVENT_BUTTON) {
            snd_seq_event_t midi_event;
            snd_seq_ev_clear(&midi_event);
            snd_seq_ev_set_source(&midi_event, port);
            snd_seq_ev_set_subs(&midi_event);
            snd_seq_ev_set_direct(&midi_event);

			if (js.value != 0) {
				midi_event.type = SND_SEQ_EVENT_NOTEON;
				midi_event.data.note.channel = 0;
				midi_event.data.note.note = 60;
				midi_event.data.note.velocity = 127;
			} else {
				midi_event.type = SND_SEQ_EVENT_NOTEOFF;
				midi_event.data.note.channel = 0;
				midi_event.data.note.note = 60;
				midi_event.data.note.velocity = 0;
			}
            snd_seq_event_output(midi, &midi_event);
            snd_seq_drain_output(midi);
		}

		/* Sleep for a short while to prevent busy waiting */
		usleep(1000);
	}
	jack_client_close(client);
    snd_seq_close(midi);
    exit(0);
}
