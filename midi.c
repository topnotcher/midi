#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "midi.h"


/** 
 * Because endianness sucks
 */
static inline uint16_t btol16(uint16_t n) {
	return (n>>8) | (n<<8);
}
static inline uint32_t btol32(uint32_t n) {
	return ((n>>24)&0xff) | ((n<<8)&0xff0000) | ((n>>8)&0xff00) |  ((n<<24)&0xff000000);
}

static inline void midi_parse_hdr(FILE * file, midi_hdr_t * hdr);
static inline void midi_parse_track(FILE * file, midi_track_t * );
static void midi_parse_track_hdr(FILE * file, midi_track_hdr_t *);



static inline midi_event_node_t * midi_parse_event(FILE * file, unsigned int * const bytes);
static inline uint32_t midi_parse_timedelta(FILE * file, unsigned int * const bytes);

static inline char * midi_get_eventstr(uint8_t cmd);


static inline void print_track(midi_track_t * const trk);
static inline void print_meta(const midi_meta_t * const meta);
static inline void print_event(const midi_event_t * const event);



static inline void do_midi_thing(char * midi_file);


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

midi_t * midi_open(char * midi_file) {
	FILE * file = fopen(midi_file, "r");

	if ( file == NULL ) {
		fprintf(stderr, "fopen(%s): epic fail\n", midi_file);
		exit(1);
	}

	midi_t * midi = malloc(sizeof *midi);
	midi->midi_file = file;

	midi_parse_hdr(file, &midi->hdr);

	//just in case there are additional bytes in the header?
	//@TODO struct alignment sucks balls.
	fseek(file, midi->hdr.hsize - (MIDI_HEADER_SIZE-4-4), SEEK_CUR);

	midi->trk_offset = ftell(file); 


	return midi;
}

void midi_close(midi_t * midi) {
	fclose(midi->midi_file);
	free(midi);
}

midi_track_t * midi_get_track(midi_t * midi, uint8_t n) {

	//seek to the beginning of the first track. 
	fseek(midi->midi_file, midi->trk_offset, SEEK_SET);

	midi_track_hdr_t trkhdr;
	
	for ( int i = 0; i < n; ++i ) {
		midi_parse_track_hdr(midi->midi_file, &trkhdr);

		//seek past the track.
		fseek(midi->midi_file, trkhdr.size, SEEK_CUR);
	}

	midi_track_t * track = malloc(sizeof *track);
	track->num = n;
	midi_parse_track(midi->midi_file, track);

	return track;	
}

void midi_free_track(midi_track_t * trk) {
	trk->cur = trk->head;

	while ( trk->cur != NULL ) {
		midi_event_node_t * prev = trk->cur;
		trk->cur = trk->cur->next;
		free(prev);
	}

	free(trk);
}

void do_midi_thing(char * midi_file) {
	
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
	
		if ( trk->cur->type == MIDI_TYPE_EVENT) {
			absolute += trk->cur->event.event.td;
			midi_event_t * event = &trk->cur->event.event;
			
			if ( event->cmd == MIDI_EVENT_NOTE_ON || event->cmd == MIDI_EVENT_NOTE_OFF )
				printf("%lu,%u,%u\n", absolute, event->data[0],event->data[1]);

		}
		else if (trk->cur->type == MIDI_TYPE_META) {
			absolute += trk->cur->event.event.td;
		}
		
		
		trk->cur = trk->cur->next;
	}
}

static inline void print_meta(const midi_meta_t * const meta) {
	printf("META (+%u) cmd: 0x%x; size: %u; data:", meta->td, meta->cmd, meta->size);

	//NOTE: this may not be ASCII, and even if it is, it is NOT null-terminated.
	for ( int b = 0; b < meta->size; ++b )
		printf("%c",meta->data[b]);

	printf("\n");

}
static inline void print_event(const midi_event_t * const event) {
	printf("%s (+%u) type: 0x%x; chan: %02x; args: ", midi_get_eventstr(event->cmd), event->td, event->cmd, event->chan);
	int args = 2;

	//@todo this is lol ugly.
	if ( event->cmd == 12 || event->cmd == 13 )
		args = 1;

	for ( int b = 0; b < args; ++b )
		printf("%d,", event->data[b]);
	printf("\n");
}



static inline void midi_parse_hdr(FILE * file, midi_hdr_t * hdr) {
	fread((void*)hdr, MIDI_HEADER_SIZE,1, file);
	hdr->hsize = btol32(hdr->hsize);
	hdr->format = btol16(hdr->format);
	hdr->tracks = btol16(hdr->tracks);
	hdr->dd = btol16(hdr->dd);
}

static void midi_parse_track_hdr(FILE * file, midi_track_hdr_t * hdr) {
	fread((void*)hdr, MIDI_TRACK_HEADER_SIZE, 1, file);
	hdr->size = btol32(hdr->size);
}

static inline void midi_parse_track(FILE * file, midi_track_t * trk) {
	midi_parse_track_hdr(file,&trk->hdr);

	unsigned int  bytes = 0;
	trk->events = 0;

	trk->head = midi_parse_event(file, &bytes);
	trk->cur = trk->head;
	trk->events += 1;

	midi_event_node_t * node = trk->head;

	while ( bytes < trk->hdr.size ) {
		node->next = midi_parse_event(file, &bytes);
		node = node->next;
		trk->events++;
	}

	node->next = NULL;
}

static inline midi_event_node_t* midi_parse_event(FILE * file, unsigned int * const bytes) {
	/**
 	 * per midi format: sometimes events will not contain  a command byte
 	 * And in this case, the "running command" from the last command byte is used.
 	 */
	static uint8_t running_cmd = 0;

	uint32_t td = midi_parse_timedelta(file, bytes);

	midi_event_node_t * node;

	uint8_t cmdchan = 0;

	*bytes += fread((void*)&cmdchan, 1,1, file);
	
	//0xFF = meta event.
	///@TODO split this out into a functionf or meta / event
	if ( cmdchan == 0xFF )  {
		uint8_t cmd ;
		uint8_t size ;
	
		//xx, nn, dd = command, length, data...
		//skip the command.
		*bytes += fread((void*)&cmd, 1,1, file);
		*bytes += fread((void*)&size, 1,1, file);

		node = malloc((sizeof *node) + size);
				
		fread(node->event.meta.data, size, 1, file);
		*bytes += size;

		node->event.meta.size = size;
		node->event.meta.cmd = cmd;
		node->event.meta.td = td;
		node->type = MIDI_TYPE_META;

	} else {
		uint8_t cmd = (cmdchan>>4)&0x0F;
		uint8_t chan = cmdchan&0x0F;
		uint8_t args[2];
		int argc = 0;
		int argn = 2;

		if ( !(cmd & 0x08) ) {
			cmd = running_cmd;
			args[argc++] = cmdchan;
		} else {
			running_cmd = cmd;
		}

		if ( !(cmd & 0x08) ) {
			printf("Invalid command (%u), but no running command. (fpos: %lu)",cmd,ftell(file));
			exit(1);
		}

		if ( cmd == 12 || cmd == 13 )
			argn--;

		node = malloc((sizeof *node) + argn);

		for ( ; argc < argn; ++argc )
			*bytes += fread(&args[argc], 1,1 , file);

		for ( int i = 0; i < argn; ++i )
			node->event.event.data[i] = args[i];
		
		node->event.event.cmd = cmd;
		node->event.event.td = td;
		node->event.event.chan = chan;
		node->type = MIDI_TYPE_EVENT;


	}

	node->next = NULL;

	return node;
}

static inline uint32_t midi_parse_timedelta(FILE * file, unsigned int  * const bytes) {

	uint8_t tmp[4] = {0};
	uint32_t td = 0;

	int read = 0;
	int more;
	do {
		fread((void*)&tmp[read],1,1, file);
		more = tmp[read]&0x80;
		tmp[read] &= 0x7F;
		read++;
	} while (more);

	//need read all the bytes first due to endianness
	for ( int i = 0; i < read; ++i ) 
		td |= tmp[i]<<((read-1-i)*7);

	*bytes += read;

	return td;
}

static inline char * midi_get_eventstr(uint8_t cmd) {
	switch(cmd) {
		case 0x8:
			return "NoteOff";
		case 0x9:
			return "NoteOn";
		case 0xa:
			return "KeyAfterTouch";
		case 0xb:
			return "ControlChange";
		case 0xc:
			return "ProgramChange";
		case 0xd:
			return "ChanAfterTouch";
		case 0xe:
			return "PitchWheelChange";
	}
	
	return "???";
}

