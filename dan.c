#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "midi.h"


static inline void do_midi_thing(char * midi_file);
static inline void print_track(midi_track_t * const);

int main(int argc, char**argv) {

	char * midi_file;
	if ( argc != 2 || strlen(argv[1]) < 1) {
		char * invocation = (argc > 0) ? argv[0] : "./midi";
		fprintf(stderr, "Usage: %s filename.mid\n\n", invocation);
		exit(1);
	} else {
		midi_file = argv[1];
	}

	do_midi_thing(midi_file);

	return 0;

}
static inline void do_midi_thing(char * midi_file) {
	
	midi_t * midi = midi_open(midi_file);	

	printf("Midi Signature: %c%c%c%c\n", midi->hdr.magic[0], midi->hdr.magic[1], midi->hdr.magic[2], midi->hdr.magic[3]);
	printf("Midi header size: %u\n", midi->hdr.hsize);
	printf("Midi format: %u\n", midi->hdr.format);
	printf("# of tracks: %u\n", midi->hdr.tracks);
	printf("delta thing: %u\n", midi->hdr.dd);

	//get the fourth track
	midi_track_t * track = midi_get_track(midi, 3);
	print_track(track);
	midi_free_track(track);
	midi_close(midi);	
}


static inline void print_track(midi_track_t * const trk) {

	printf("Track %d, %d events, %u bytes, sig: %c%c%c%c\n", trk->num, trk->events, trk->hdr.size, 
		trk->hdr.magic[0], trk->hdr.magic[1], trk->hdr.magic[2], trk->hdr.magic[3]);

	trk->cur = trk->head;

		
	unsigned long int absolute = 0;
	while ( trk->cur != NULL ) {
	
		if ( trk->cur->event.type == MIDI_TYPE_EVENT) {
			absolute += trk->cur->event.td;
			midi_event_t * event = &trk->cur->event;
			
			if ( event->cmd == MIDI_EVENT_NOTE_ON || event->cmd == MIDI_EVENT_NOTE_OFF )
				printf("%lu,%u,%u\n", absolute, event->data[0],event->data[1]);

		}
		else if (trk->cur->event.type == MIDI_TYPE_META) {
			absolute += trk->cur->event.td;
		}
		
		
		trk->cur = trk->cur->next;
	}
}
