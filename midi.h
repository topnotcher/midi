#include <stdint.h>



#define MIDI_HEADER_SIZE 14
typedef struct {
	char magic[4];
	uint32_t hsize;
	uint16_t format;
	uint16_t tracks;
	uint16_t dd;
} midi_hdr_t;

#define MIDI_TRACK_HEADER_SIZE 8
typedef struct {
	char magic[4];
	uint32_t size;
} midi_track_hdr_t;

typedef struct {
	midi_track_hdr_t hdr; 		
} midi_track_t;

typedef struct {
	uint32_t t; //time delta...
	uint8_t cmd;
	uint8_t chan;
	uint8_t * data; 
} midi_event_t;
