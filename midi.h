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
	uint8_t cmd;
	uint8_t size;
	uint8_t data[];
} midi_meta_t;

typedef struct {
	uint8_t cmd;
	uint8_t chan;
	uint8_t data[]; 
} midi_event_t;


typedef struct midi_event_node_s {
	uint32_t td;
	enum {
		MIDI_TYPE_EVENT,
		MIDI_TYPE_META
	} type;
	struct midi_event_node_s * next;
	union {
		midi_event_t event;
		midi_meta_t meta;
	} event;
} midi_event_node_t;


typedef struct {
	midi_track_hdr_t hdr;
	uint32_t events;
	midi_event_node_t * head;
	midi_event_node_t * cur;
} midi_track_t;


#define MIDI_EVENT_NOTE_OFF 		0x08
#define MIDI_EVENT_NOTE_ON 		0x09
#define MIDI_EVENT_KEY_AFTER_TOUCH	0x0A
#define MIDI_EVENT_CONTROL_CHANGE	0x0B
#define MIDI_EVENT_PATH_CHANGE		0x0C
#define MIDI_EVENT_CHANNEL_AFTER_TOUCH	0x0D
#define MIDI_EVENT_PITCH_WHEEL_CHANGE	0x0E
