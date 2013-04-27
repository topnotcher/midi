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

static inline midi_event_node_t * midi_parse_event(FILE * file, unsigned int * const bytes);
static inline uint32_t midi_parse_timedelta(FILE * file, unsigned int * const bytes);

static inline char * midi_get_eventstr(uint8_t cmd);

static inline void print_track(midi_track_t * const trk);

int main(void) {
	char midi_file[] = "iloverocknroll.mid";

	FILE * midi = fopen(midi_file, "r");

	midi_hdr_t hdr;		
	midi_parse_hdr(midi, &hdr);

	//just in case there are additional bytes in the header?
	//@TODO struct alignment sucks balls.
	fseek(midi, hdr.hsize - (MIDI_HEADER_SIZE-4-4), SEEK_CUR);


	printf("Midi Signature: %c%c%c%c\n", hdr.magic[0], hdr.magic[1], hdr.magic[2], hdr.magic[3]);
	printf("Midi header size: %u\n", hdr.hsize);
	printf("Midi format: %u\n", hdr.format);
	printf("# of tracks: %u\n", hdr.tracks);
	printf("delta thing: %u\n", hdr.dd);

	midi_track_t tracks[hdr.tracks];

	for ( int i = 0; i < hdr.tracks; ++i ) {
		midi_parse_track(midi, &tracks[i]);

		print_track(&tracks[i]);
	}

	
	fclose(midi);
}

static inline void print_track(midi_track_t * const trk) {

	printf("Track %d, %d events, %u bytes, sig: %c%c%c%c\n", trk->num, trk->events, trk->hdr.size, 
		trk->hdr.magic[0], trk->hdr.magic[1], trk->hdr.magic[2], trk->hdr.magic[3]);

	trk->cur = trk->head;

		
	while ( trk->cur != NULL ) {
		if ( trk->cur->type == MIDI_TYPE_META ) {
			printf("META (+%u) cmd: 0x%x; size: %u; data:", trk->cur->td, trk->cur->event.meta.cmd, trk->cur->event.meta.size);

			for ( int b = 0; b < trk->cur->event.meta.size; ++b )
				printf("%c",trk->cur->event.meta.data[b]);

		} else if ( trk->cur->type == MIDI_TYPE_EVENT ) {
			printf("EVENT (+%u) type: 0x%x; chan: %02x; args: ", trk->cur->td, trk->cur->event.event.cmd, trk->cur->event.event.chan);
			int args = 2;
			//@todo this is lol ugly.
			if ( trk->cur->event.event.cmd == 12 || trk->cur->event.event.cmd == 13 )
				args = 1;

			for ( int b = 0; b < args; ++b )
				printf("%d ", trk->cur->event.event.data[b]);
		}
			
		printf("\n");
		
		midi_event_node_t * prev = trk->cur;
		trk->cur = trk->cur->next;

		//at present, only the events are on the heap.
		free(prev);
	}
}


static inline void midi_parse_hdr(FILE * file, midi_hdr_t * hdr) {
	fread((void*)hdr, MIDI_HEADER_SIZE,1, file);
	hdr->hsize = btol32(hdr->hsize);
	hdr->format = btol16(hdr->format);
	hdr->tracks = btol16(hdr->tracks);
	hdr->dd = btol16(hdr->dd);
}

static inline void midi_parse_track(FILE * file, midi_track_t * trk) {
	fread((void*)&trk->hdr, MIDI_TRACK_HEADER_SIZE, 1, file);
	trk->hdr.size = btol32(trk->hdr.size);

	unsigned int  bytes = 0;
	trk->events = 0;

	//@TODO check return
	trk->head = midi_parse_event(file, &bytes);
	trk->cur = trk->head;
	trk->events += 1;

	midi_event_node_t * node = trk->head;

	while ( bytes < trk->hdr.size ) {
	//	printf("%u bytes\n", bytes);
		node->next = midi_parse_event(file, &bytes);
		node = node->next;
		trk->events++;
	}

	node->next = NULL;
}

static inline midi_event_node_t* midi_parse_event(FILE * file, unsigned int * const bytes) {
	//printf("****[1] %u\n", *bytes);
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

		//printf("Read %u meta bytes\n",size);
		node->event.meta.size = size;
		node->event.meta.cmd = cmd;
		node->type = MIDI_TYPE_META;
	//	printf("***M[2] %u\n", *bytes);

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
		node->event.event.chan = chan;
		node->type = MIDI_TYPE_EVENT;

	//	printf("***E[2] %u\n", *bytes);

	}

	node->next = NULL;
	node->td = td;

	return node;
}

static inline uint32_t midi_parse_timedelta(FILE * file, unsigned int  * const bytes) {

	uint8_t tmp = 0;
	uint32_t td = 0;

	int read = 0;
	for ( int done = 0; !done && read < sizeof(uint32_t); read += 1 ) {
		fread((void*)&tmp,1,1, file);

		if ( !(tmp & 0x80) ) 
			done = 1;
		else 
			tmp &= 0x7F;

		td |= tmp<<(7*read);
	}
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


/*static inline uint8_t midi_parse_command(FILE * file) {
	
}*/

