#include <stdio.h>
#include <string.h>
#include "midi.h"


static inline uint16_t btol16(uint16_t n) {
	return (n>>8) | (n<<8);
}

static inline uint32_t btol32(uint32_t n) {
	return  ((n>>24)&0xff) | ((n<<8)&0xff0000) | ((n>>8)&0xff00) |  ((n<<24)&0xff000000);
}

static inline void midi_parse_hdr(FILE * file, midi_hdr_t * hdr);
static inline void midi_parse_track(FILE * file, midi_track_t * hdr);


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
		printf("track signature %d: %c%c%c%c\n",i, tracks[i].hdr.magic[0], tracks[i].hdr.magic[1], tracks[i].hdr.magic[2], tracks[i].hdr.magic[3]);

		fseek(midi, tracks[i].hdr.size, SEEK_CUR);

	}

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
}
