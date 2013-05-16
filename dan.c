#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "midi.h"

#define MIN(a,b) ((a>b)?b:a)
#define MAX(a,b) ((a<b)?b:a)

/**
 * This maps track names to outputted filenames.
 */
static struct {
	const char * part;
	const char * suffix;
} track_map[] = {
	{ .part = "PART GUITAR",
	  .suffix = ".guitar",
	},
	{ .part = "BEAT",
	  .suffix = ".beat",
	},
	{ .part = "PART VOCALS",
	  .suffix = ".vocals",
	},
	{ .part = "PART BASS",
	  .suffix = ".bass",
	},
	{ .part = "PART DRUMS",
	  .suffix = ".drums",
	},

	//leave this last. Using it to stop iteration.
	{ .part = NULL }
};

#define TIME_SUFFIX ".time"

static inline int do_midi_thing(char * midi_file);
static inline int output_track(midi_track_t * const, char * file);
static inline int output_time(midi_t * const, char * file);
static inline char * part_filename(char const * str, const char const * file, const char const * suffix);
static inline midi_track_t * find_track(midi_t * midi, const char const * part);

int main(int argc, char**argv) {

	char * midi_file;
	if ( argc != 2 || strlen(argv[1]) < 1) {
		char * invocation = (argc > 0) ? argv[0] : "./midi";
		fprintf(stderr, "Usage: %s filename.mid\n\n", invocation);
		return 1;
	} else {
		midi_file = argv[1];
	}

	return do_midi_thing(midi_file);

}

static inline int do_midi_thing(char * midi_file) {

	midi_t * midi = midi_open(midi_file);	

	int retn = 0;

	if ( midi == NULL ) {
		fprintf(stderr, "Failed open midi file.\n");
		return 1;
	}

	//find the maximum length of all the filename suffixes.
	int max_suffix_len = 0;
	
	for ( int i = 0; track_map[i].part != NULL; ++i )
		if ( strlen(track_map[i].suffix) > max_suffix_len )
			max_suffix_len = strlen(track_map[i].suffix);	


	char * partfile = malloc(strlen(midi_file)+max_suffix_len+1);


	for ( int i = 0; track_map[i].part != NULL; ++i ) {
		midi_track_t * trk = find_track(midi, track_map[i].part);
		
		if ( trk == NULL ) {
			fprintf(stderr, "Failed to find %s\n", track_map[i].part);
			retn = 1;
			continue;
		} 

		partfile = part_filename(partfile,midi_file,track_map[i].suffix);

		if ( output_track(trk, partfile) ) {
			fprintf(stderr, "Failed to output %s to %s\n", track_map[i].part, partfile); 
			retn = 1;
		}

		midi_free_track(trk);
	}

	if ( output_time(midi,part_filename(partfile,midi_file,TIME_SUFFIX)) ) {
		fprintf(stderr, "failed to output time\n");
		retn = 1;
	}

	//end:
	midi_close(midi);

	return retn;	

}

static inline char * part_filename(char const * str, const char const * file, const char const * suffix) {
	strncpy((char *)str, (char *)file, strlen(file)+1);
	strncat((char *)str, (char *)suffix, strlen(suffix)+1);
	return (char*)str;
}

static inline int output_track(midi_track_t * const trk, char* fname) {

//	printf("Track %d, %d events, %u bytes, sig: %c%c%c%c\n", trk->num, trk->events, trk->hdr.size, 
//		trk->hdr.magic[0], trk->hdr.magic[1], trk->hdr.magic[2], trk->hdr.magic[3]);

	FILE * file = fopen(fname, "w+");
	
	if ( file == NULL )
		return 1;

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
				fprintf(file, "%lu,%u,%u\n", absolute, event->data[0],event->data[1]);

		} else if (cur->type == MIDI_TYPE_META) {
			absolute += cur->td;
		}	
	}
		
	fclose(file);

	return 0;
}

static inline int output_time(midi_t * const midi, char * fname) {

	midi_track_t * trk =  midi_get_track(midi, 0);

	if ( trk == NULL ) {
		fprintf(stderr, "Failed to find track 0... WTF?\n");
		return 1;
	}

	FILE * file = fopen(fname, "w+");
	
	if ( file == NULL )
		return 1;


	midi_iter_track(trk);
	unsigned long int absolute = 0;
	while ( midi_track_has_next(trk) ) {
		midi_event_t * cur = midi_track_next(trk);
		absolute += cur->td;	

		if ( cur->type == MIDI_TYPE_META && cur->cmd == 0x58 ) 
			fprintf(file, "%lu,%u,%u\n", absolute,cur->data[0], cur->data[1]);
	}

	midi_free_track(trk);

	return 0;
}


//this is a pretty inefficient way of doing this, but I don't really feel like making
//the midi code "public" and using it mroe... besides this is easier anyway!

static inline midi_track_t * find_track(midi_t * midi, const char const * part) {
	midi_track_t * track;

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
	
			if ( evnt->size == strlen(part) 
			   &&  !strncmp((const char *)evnt->data, part, evnt->size) ) {
				return track;
			}  	
		}
		
		if ( track != NULL )
			midi_free_track(track);
	}

	return NULL;
}
