#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <jack/jack.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/joystick.h>
#include <jack/ringbuffer.h>
//#include <libevdev-1.0/libevdev/libevdev.h>
#include <alsa/asoundlib.h>

/*

gcc -o sine_jack_joy_midi sine_jack_joy_midi.c -ljack -lm -lasound


*/





jack_port_t *output_port;
jack_client_t *client;
double amplitude = 0.5;
double frequency = 440.0;
double phase = 0.0;

int process(jack_nframes_t nframes, void *arg) {
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

    return 0;
}

int main() {
    // open joystick device
    int joystick_fd = open("/dev/input/js0", O_RDONLY);

    // check if joystick device was opened successfully
    if (joystick_fd == -1) {
        printf("Error: Failed to open joystick device.\n");
        return 1;
    }

    // initialize joystick input
    struct js_event js;
    int bytes_read;

    jack_nframes_t buffer_size;
    float *audio_buffer;

    // Obtain the buffer size from JACK
    buffer_size = jack_get_buffer_size(client);

    // Allocate memory for the buffer
    audio_buffer = malloc(buffer_size * sizeof(float));

    if (!audio_buffer) {
        printf("Failed to allocate memory for audio buffer.\n");
        exit(1);
    }

    jack_ringbuffer_t *output_ringbuffer;
    output_ringbuffer = jack_ringbuffer_create(buffer_size * sizeof(float));

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

    output_port = jack_port_register(client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

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

	// Create MIDI number note array
	const int NOTE_MAP[] = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76};
	int channel = 0;
	int invert_axis = 1;

	int note_index = -1;


	while (1) {

        // Translate input event into MIDI message
        snd_seq_event_t midi_event;
        snd_seq_ev_clear(&midi_event);
        snd_seq_ev_set_source(&midi_event, port);
        snd_seq_ev_set_subs(&midi_event);
        snd_seq_ev_set_direct(&midi_event);






		/* Read joystick events */
		if (read(joystick_fd, &js, sizeof(js)) == -1) {
		    /* Error reading joystick */
		    break;
		}

		/* Check if it is an axis event */
		if (js.type == JS_EVENT_AXIS && js.number == 0) {		    
	        /* Update frequency based on joystick position */
	        frequency = ((js.value + 32767) / 3276.7);
		}
		if (js.type == JS_EVENT_AXIS && js.number == 1) {
		    /* Update frequency based on joystick position */
		    amplitude = abs(js.value / 32767);
		}





		if (js.type == JS_EVENT_BUTTON) {
	        /* Update frequency based on joystick position */
	        //amplitude = abs(js.value / 32767);
	    
			note_index = js.number;
	
/*			switch (js.number) {
				case ABS_HAT0X: note_index = 12; break;
				case ABS_HAT0Y:	note_index = 13; break;
				case ABS_Z: note_index = 14; break;
				case ABS_RZ: note_index = 15; break;
			}    */
   			

			if (js.value != -1) {
				if (js.value != 0) {
					midi_event.type = SND_SEQ_EVENT_NOTEON;
					midi_event.data.note.channel = 0;
					midi_event.data.note.note = NOTE_MAP[note_index];
					midi_event.data.note.velocity = 127;
				} else {
					midi_event.type = SND_SEQ_EVENT_NOTEOFF;
					midi_event.data.note.channel = 0;
					midi_event.data.note.note = NOTE_MAP[note_index];
					midi_event.data.note.velocity = 0;
				}
			}

		}

		/* Process audio buffer */
		process(buffer_size, client);

		/* Write audio buffer to output */
		jack_default_audio_sample_t *out;
		out = jack_port_get_buffer(output_port, buffer_size);
		jack_ringbuffer_read(output_ringbuffer, (void *)out, buffer_size * sizeof(float));

        snd_seq_event_output(midi, &midi_event);
        snd_seq_drain_output(midi);



		/* Sleep for a short while to prevent busy waiting */
		usleep(1000);
	}
	jack_client_close(client);
    snd_seq_close(midi);

    exit(0);
}
