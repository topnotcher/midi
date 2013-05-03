#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "midi.h"

#define MIN(a,b) ((a>b)?b:a)

#define PART_GUITAR (const char *)"PART GUITAR"
#define PART_BEAT (const char *)"BEAT"


static inline void do_midi_thing(char * midi_file);
static inline void print_track(midi_track_t * const);
static inline void find_tracks(midi_t * midi, midi_track_t ** guitar, midi_track_t ** beat);

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

//	printf("Midi Signature: %c%c%c%c\n", midi->hdr.magic[0], midi->hdr.magic[1], midi->hdr.magic[2], midi->hdr.magic[3]);
//	printf("Midi header size: %u\n", midi->hdr.hsize);
//	printf("Midi format: %u\n", midi->hdr.format);
//	printf("# of tracks: %u\n", midi->hdr.tracks);
//	printf("Ticks per beat: %u\n", midi->hdr.dd);

	midi_track_t * guitar = NULL;
	midi_track_t * beat = NULL;


	find_tracks(midi,&guitar,&beat);
		
	if ( guitar != NULL ) {
		//printf("Found PART GUITAR, track: %u\n", guitar->num);
	} else {
		fprintf(stderr, "Failed to find PART GUITAR\n");
		goto end;
	}

	if ( beat != NULL ) {
		//printf("Found BEAT, track: %u\n", beat->num);
	} else {
		fprintf(stderr, "Failed to find PART GUITAR\n");
		goto end;
	}
	


	print_track(guitar);
	print_track(beat);

	//get the fourth track
//	midi_track_t * track = midi_get_track(midi, 3);
//	print_track(track);
//	midi_free_track(track);
//

	end:
	if ( guitar != NULL ) midi_free_track(guitar);
	if ( beat != NULL ) midi_free_track(beat);
	midi_close(midi);	
}


static inline void print_track(midi_track_t * const trk) {

//	printf("Track %d, %d events, %u bytes, sig: %c%c%c%c\n", trk->num, trk->events, trk->hdr.size, 
//		trk->hdr.magic[0], trk->hdr.magic[1], trk->hdr.magic[2], trk->hdr.magic[3]);

	trk->cur = trk->head;

		
	unsigned long int absolute = 0;

	midi_iter_track(trk);
	midi_event_t * cur;
	while ( midi_track_has_next(trk) ) {
		cur = midi_track_next(trk);
		if ( cur->type == MIDI_TYPE_EVENT) {
			absolute += cur->td;
			midi_event_t * event = cur;
			
			if ( event->cmd == MIDI_EVENT_NOTE_ON || event->cmd == MIDI_EVENT_NOTE_OFF )
				printf("%lu,%u,%u\n", absolute, event->data[0],event->data[1]);

		} else if (cur->type == MIDI_TYPE_META) {
			absolute += cur->td;
		}	
	}
}

//this is a pretty inefficient way of doing this, but I don't really feel like making
//the midi code "public" and using it mroe... besides this is easier anyway!

static inline void find_tracks(midi_t * midi, midi_track_t ** guitar, midi_track_t ** beat) {
	midi_track_t * track;

	*beat = *guitar = NULL;


	for ( int i = 0; i < midi->hdr.tracks; ++i ) {
		track =  midi_get_track(midi, i);


//		printf("Track %d, %d events, %u bytes, sig: %c%c%c%c\n", track->num, track->events, track->hdr.size, 
//			track->hdr.magic[0], track->hdr.magic[1], track->hdr.magic[2], track->hdr.magic[3]);

		midi_iter_track(track);
		midi_event_t * evnt;
		while ( midi_track_has_next(track) ) {
			evnt = midi_track_next(track);

			if ( evnt->td != 0 )
				break;
			//0x03 = track name :D
			if ( evnt->type != MIDI_TYPE_META || evnt->cmd != 0x03 ) 
				continue;

			//for ( int i = 0; i < evnt->size; ++i )
			//	printf("%c%u", (char)evnt->data[i],evnt->data[i]);

		/*	printf("strncmp: %d, size:%d, len:%d\n", 
				strncmp((const char *)evnt->data, PART_GUITAR, evnt->size),
				evnt->size,
				(int)strlen(PART_GUITAR)
			);
		*/	
			if ( evnt->size == strlen(PART_GUITAR) 
			   &&  !strncmp((const char *)evnt->data, PART_GUITAR, evnt->size) ) {
				*guitar = track;

			} else if ( evnt->size == strlen(PART_BEAT)
			    && !strncmp((const char *)evnt->data, PART_BEAT, evnt->size) ) {
				*beat = track;
			}  else {
				midi_free_track(track);
			}
			
			track = NULL;
			break;
		}
		
		if ( track != NULL )
			midi_free_track(track);
	}
}

