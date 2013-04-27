#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "midi.h"


static inline uint16_t btol16(uint16_t n) {
	return (n>>8) | (n<<8);
}

static inline uint32_t btol32(uint32_t n) {
	return  ((n>>24)&0xff) | ((n<<8)&0xff0000) | ((n>>8)&0xff00) |  ((n<<24)&0xff000000);
}

static inline void midi_parse_hdr(FILE * file, midi_hdr_t * hdr);
static inline void midi_parse_track(FILE * file, midi_track_t * hdr);

static inline int midi_parse_event(FILE * file);
static inline int midi_parse_timedelta(FILE * file, uint32_t * td);
//static inline uint8_t midi_parse_command(FILE * file);



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
	printf("Header parsed; fpos: %lu\n",ftell(midi));
	for ( int i = 0; i < hdr.tracks; ++i ) 
		midi_parse_track(midi, &tracks[i]);
//fseek(midi, tracks[i].hdr.size, SEEK_CUR);

	fclose(midi);
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

	printf("--- Process track ---\n");
	printf("Track signature: %c%c%c%c\n", trk->hdr.magic[0], trk->hdr.magic[1], trk->hdr.magic[2], trk->hdr.magic[3]);


	unsigned int bytes = 0;

	while ( bytes < trk->hdr.size ) {
		bytes += midi_parse_event(file);
	}

	printf("*** bytes: %u, size: %u\n", bytes, trk->hdr.size);
	printf("*** file is at %lu\n" , ftell(file));

}

static inline int midi_parse_event(FILE * file) {
	
	static uint8_t running_cmd = 0;

	unsigned int bytes = 0;

	uint32_t td;
	bytes += midi_parse_timedelta(file, &td);

	printf("time delta: %u\n", td);

	uint8_t cmdchan = 0;

	bytes+=fread((void*)&cmdchan, 1,1, file);

	//meta event
	
	if ( cmdchan == 0xFF )  {
		uint8_t tmp ;
		//xx, nn, dd = command, length, data...
		//skip the command.
		bytes += fread((void*)&tmp, 1,1, file);
		bytes += fread((void*)&tmp, 1,1, file);
//		printf("fpos: %lu\n", ftell(file));
		printf("Skipping %u meta bytes\n", tmp);
		bytes += tmp;
		fseek(file, tmp, SEEK_CUR);


	} else {
		uint8_t cmd = (cmdchan>>4)&0x0F;
		uint8_t chan = cmdchan&0x0F;
		int skipbytes = 2;

		if ( !(cmd & 0x08) ) {
			cmd = running_cmd;
			skipbytes--;
		}

		else 
			running_cmd = cmd;

		if ( !(cmd & 0x08) ) {
			printf("Invalid command, but no running command.");
			exit(1);
		}


		printf("command: %u, chan: %u (0x%02x)\n",cmd, chan,cmdchan);


		
		if ( cmd == 12 || cmd == 13 )
			skipbytes = 1;

		bytes += skipbytes;
		fseek(file, skipbytes, SEEK_CUR);
	}
//	exit(0);

	return bytes;
}

static inline int midi_parse_timedelta(FILE * file, uint32_t * td) {

	uint8_t tmp = 0;
	*td = 0;

	int bytes = 0;
	for ( int done = 0; !done && bytes < sizeof(uint32_t); bytes += 1 ) {
		fread((void*)&tmp,1,1, file);
		printf("Rd:%u\n",tmp)	;
		if ( !(tmp & 0x80) ) 
			done = 1;
		else 
			tmp &= 0x7F;

		*td |= tmp<<(7*bytes);
	}
	printf("read %d bytes of td\n",bytes);
	return bytes;
}

/*static inline uint8_t midi_parse_command(FILE * file) {
	
}*/

