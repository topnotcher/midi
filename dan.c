#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "midi.h"

#define MIN(a,b) ((a>b)?b:a)
#define MAX(a,b) ((a<b)?b:a)


#define PART_GUITAR (const char *)"PART GUITAR"
#define PART_BEAT (const char *)"BEAT"
#define PART_VOCALS (const char*)"PART VOCALS"
#define PART_BASS (const char*)"PART BASS"
#define PART_DRUMS (const char*)"PART DRUMS"

//max length of any of these suffixes.
#define MAX_SUFFIX_LEN 10 
#define GUITAR_SUFFIX ".guitar"
#define BEAT_SUFFIX ".beat"
#define VOCALS_SUFFIX ".vocals"
#define BASS_SUFFIX  ".bass"
#define DRUMS_SUFFIX ".drums"

static inline int do_midi_thing(char * midi_file);
static inline int output_track(midi_track_t * const, char * file);
static inline char * part_filename(char * str, char * file, char * suffix);

static inline void find_tracks(midi_t * midi, 
	midi_track_t ** guitar, 
	midi_track_t ** beat, 
	midi_track_t ** vocals,
	midi_track_t ** drums,
	midi_track_t ** bass

);

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

//	printf("Midi Signature: %c%c%c%c\n", midi->hdr.magic[0], midi->hdr.magic[1], midi->hdr.magic[2], midi->hdr.magic[3]);
//	printf("Midi header size: %u\n", midi->hdr.hsize);
//	printf("Midi format: %u\n", midi->hdr.format);
//	printf("# of tracks: %u\n", midi->hdr.tracks);
//	printf("Ticks per beat: %u\n", midi->hdr.dd);

	midi_track_t * guitar = NULL;
	midi_track_t * beat = NULL;
	midi_track_t * vocals = NULL;
	midi_track_t * bass = NULL;
	midi_track_t * drums = NULL;

	find_tracks(midi,&guitar,&beat,&vocals,&drums,&bass);
		
	if ( guitar == NULL ) {
		fprintf(stderr, "Failed to find PART GUITAR\n");
		retn = 1;
		goto end;
	} else if ( beat == NULL ) {
		fprintf(stderr, "Failed to find BEAT\n");
		retn = 1;
		goto end;
	} else if ( vocals == NULL ) {
		fprintf(stderr, "Failed to find VOCALST\n");
		retn = 1;
		goto end;
	} else if ( bass == NULL ) {
		fprintf(stderr, "Failed to find BASS\n");
		retn = 1;
		goto end;
	} else if ( drums == NULL ) {
		fprintf(stderr, "Failed to find DRUMS\n");
		retn = 1;
		goto end;
	}

	char * partfile = malloc(strlen(midi_file)+MAX_SUFFIX_LEN+1);

	if ( output_track(guitar, part_filename(partfile,midi_file,GUITAR_SUFFIX)) ) {
		fprintf(stderr, "Failed to output guitar track\n");
		retn = 1;

	} else if ( output_track(beat, part_filename(partfile,midi_file,BEAT_SUFFIX)) )  {
		fprintf(stderr, "Failed to output beat track\n");
		retn = 1;


	} else if ( output_track(drums, part_filename(partfile,midi_file,DRUMS_SUFFIX)) )  {
		fprintf(stderr, "Failed to output drums track\n");
		retn = 1;

	} else if ( output_track(beat, part_filename(partfile,midi_file,VOCALS_SUFFIX)) )  {
		fprintf(stderr, "Failed to output vocals track\n");
		retn = 1;

	} else if ( output_track(beat, part_filename(partfile,midi_file,BASS_SUFFIX)) )  {
		fprintf(stderr, "Failed to output bass track\n");
		retn = 1;
	}

	free(partfile);
	
	end:	
	if ( guitar != NULL ) midi_free_track(guitar);
	if ( beat != NULL ) midi_free_track(beat);
	if ( bass != NULL ) midi_free_track(bass);
	if ( vocals != NULL ) midi_free_track(vocals);
	if ( drums != NULL ) midi_free_track(drums);
	midi_close(midi);
	return retn;	
}

static inline char * part_filename(char * str, char * file, char * suffix) {
	strncpy(str, file, strlen(file)+1);
	strncat(str, suffix, strlen(suffix)+1);
	return str;
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

//this is a pretty inefficient way of doing this, but I don't really feel like making
//the midi code "public" and using it mroe... besides this is easier anyway!

static inline void find_tracks(midi_t * midi, 
	midi_track_t ** guitar, 
	midi_track_t ** beat, 
	midi_track_t ** vocals,
	midi_track_t ** drums,
	midi_track_t ** bass

) {
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
	
			if ( evnt->size == strlen(PART_GUITAR) 
			   &&  !strncmp((const char *)evnt->data, PART_GUITAR, evnt->size) ) {
				*guitar = track;

			} else if ( evnt->size == strlen(PART_BEAT)
			    && !strncmp((const char *)evnt->data, PART_BEAT, evnt->size) ) {
				*beat = track;

			} else if ( evnt->size == strlen(PART_VOCALS)
			    && !strncmp((const char *)evnt->data, PART_VOCALS, evnt->size) ) {
				*vocals = track;

			} else if ( evnt->size == strlen(PART_DRUMS)
			    && !strncmp((const char *)evnt->data, PART_DRUMS, evnt->size) ) {
				*drums = track;

			} else if ( evnt->size == strlen(PART_BASS)
			    && !strncmp((const char *)evnt->data, PART_BASS, evnt->size) ) {
				*bass = track;

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

