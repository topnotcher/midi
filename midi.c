#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include "midi.h"

const uint8_t MIDI_MAGIC[] = {'M','T','h','d'};
const uint8_t MIDI_TRACK_MAGIC[] = {'M','T','r','k'};

static inline uint16_t btol16(const uint16_t n) {
	return (n>>8) | (n<<8);
}

static inline uint32_t btol32(const uint32_t n) {
	return ((n>>24)&0xff) | ((n<<8)&0xff0000) | ((n>>8)&0xff00) |  ((n<<24)&0xff000000);
}

static bool midi_parse_hdr(midi_t *const);
static bool midi_parse_track(const midi_t *const midi, midi_track_t *);
static bool midi_parse_track_hdr(const midi_t *const, midi_track_hdr_t *);
static void midi_set_error(midi_t *, int, const char *const, ...);
static void midi_prefix_errmsg(midi_t *, const char *const, ...);

static bool midi_check_magic(const uint8_t *const, const uint8_t *const, const size_t);

static inline midi_event_node_t * midi_parse_event(const midi_t *const, unsigned int * const bytes);
static inline uint32_t midi_parse_timedelta(FILE * file, unsigned int * const bytes);

/**
 * Open a midi file given by the midi_file parameter.
 *
 * On success, 0 is returned and *midi = a (midi_t *) handle to the opened midi
 * file. On error, a POSIX errno is returned and *midi is NULL;
 *
 * On error, NULL is returned
 */
int midi_open(const char *const midi_file, midi_t **midi) {
	FILE *file = NULL;
	*midi = NULL;
	int status;

	file = fopen(midi_file, "r");
	if (file == NULL) {
		return errno;
	}

	*midi = calloc(sizeof **midi, 1);

	if (*midi != NULL) {
		(*midi)->midi_file = file;

		if (!midi_parse_hdr(*midi)) {
			midi_close(*midi);

			return EINVAL;
		} else {
			//just in case there are additional bytes in the header?
			status = fseek((*midi)->midi_file, (*midi)->hdr.hsize - (MIDI_HEADER_SIZE - 4 - 4), SEEK_CUR);

			if (status != -1) {
				(*midi)->trk_offset = ftell((*midi)->midi_file);
				return 0;
			} else {
				return errno;
			}
		}
	} else {
		fclose(file);
		return ENOMEM;
	}
}

void midi_close(midi_t *midi) {
	if (midi != NULL && midi->midi_file != NULL) {
		fclose(midi->midi_file);
	}

	free(midi);
}

/**
 * Retrieve a MIDI track (midi_track_t*) including the track header. Suitable
 * for iteration with midi_iter_track.
 */
midi_track_t *midi_get_track(const midi_t *const midi, uint8_t track_idx) {
	int status;
	midi_track_t *track = NULL;

	//TODO
	status = fseek(midi->midi_file, midi->trk_offset, SEEK_SET);

	if (status == -1) {
		midi_set_error((midi_t*)midi, errno, "fseek() failed.");
		return NULL;
	}

	midi_track_hdr_t trkhdr;

	for (int i = 0; i < track_idx; ++i) {
		if (midi_parse_track_hdr(midi, &trkhdr)) {
			//seek past the track.
			status = fseek(midi->midi_file, trkhdr.size, SEEK_CUR);

			if (status == -1) {
				midi_set_error((midi_t*)midi, errno, "fseek() failed to seek past track %d header.", track_idx);
				return NULL;
			}

		} else {
			midi_prefix_errmsg((midi_t*)midi, "Failed to parse track %d header");
			return NULL;
		}
	}

	track = malloc(sizeof *track);
	if (track != NULL) {
		track->num = track_idx;

		if (!midi_parse_track(midi, track)) {
			midi_free_track(track);
			track = NULL;
		}

	}

	return track;
}

/**
 * Free a track previously allocated with midi_get_track()
 */
void midi_free_track(midi_track_t *trk) {
	if (trk == NULL)
		return;

	trk->cur = trk->head;
	while (trk->cur != NULL) {
		midi_event_node_t  *cur = trk->cur;
		trk->cur = trk->cur->next;
		free(cur);
	}

	free(trk);
}

/**
 * Read and parse the midi header from file into a midi_hdr_t structure.
 */
static bool midi_parse_hdr(midi_t *const midi) {
	uint8_t buf[MIDI_HEADER_SIZE] = {0};
	midi_hdr_t *hdr = &midi->hdr;
	size_t ret = fread(buf, MIDI_HEADER_SIZE, 1, midi->midi_file);

	if (ret == 1 && midi_check_magic(MIDI_MAGIC, buf, sizeof(MIDI_MAGIC))) {
		memcpy(&hdr->magic, buf + MIDI_HEADER_MAGIC_OFFSET, sizeof(hdr->magic));
		hdr->hsize = btol32(*(uint32_t*)(buf + MIDI_HEADER_HSIZE_OFFSET));
		hdr->format = btol16(*(uint16_t*)(buf + MIDI_HEADER_FORMAT_OFFSET));
		hdr->tracks = btol16(*(uint16_t*)(buf + MIDI_HEADER_TRACKS_OFFSET));
		hdr->dd = btol16(*(uint16_t*)(buf + MIDI_HEADER_DD_OFFSET));

		return true;
	} else {
		return false;
	}
}

static bool midi_parse_track_hdr(const midi_t *const midi, midi_track_hdr_t *hdr) {
	uint8_t buf[MIDI_TRACK_HEADER_SIZE] = {0};
	size_t ret = fread(buf, MIDI_TRACK_HEADER_SIZE, 1, midi->midi_file);

	if (ret == 1) {
		memcpy(&hdr->magic, buf + MIDI_TRACK_HEADER_MAGIC_OFFSET, sizeof(hdr->magic));
		hdr->size = btol32(*(uint32_t*)(buf + MIDI_TRACK_HEADER_SIZE_OFFSET));

		if (midi_check_magic(MIDI_TRACK_MAGIC, hdr->magic, sizeof(MIDI_TRACK_MAGIC))) {
			return true;
		} else {
			midi_set_error((midi_t*)midi, errno, "track has bad magic.");
			return false;
		}

	} else {
		midi_set_error((midi_t*)midi, errno, "fread() failed to read track header.");
		return false;
	}
}

static bool midi_parse_track(const midi_t *const midi, midi_track_t *trk) {
	midi_event_node_t *node;
	bool parsed = midi_parse_track_hdr(midi, &trk->hdr);

	if (!parsed) {
		return false;
	}

	unsigned int bytes = 0;
	trk->events = 0;

	node = midi_parse_event(midi, &bytes);

	if (node == NULL) {
		return false;
	}

	trk->head = node;
	trk->cur = trk->head;
	trk->events += 1;

	node = trk->head;

	while (bytes < trk->hdr.size) {
		node->next = midi_parse_event(midi, &bytes);
		if (node->next != NULL) {
			node = node->next;
			trk->events++;
		} else {
			return false;
		}
	}

	node->next = NULL;

	return true;
}

static inline midi_event_node_t *midi_parse_event(const midi_t *const midi, unsigned int *const bytes) {
	/**
 	 * per midi format: sometimes events will not contain  a command byte
 	 * And in this case, the "running command" from the last command byte is used.
 	 */
	static uint8_t running_cmd = 0;

	uint32_t td = midi_parse_timedelta(midi->midi_file, bytes);

	midi_event_node_t * node;

	uint8_t cmdchan = 0;

	*bytes += fread(&cmdchan, 1,1, midi->midi_file);

	//0xFF = meta event.
	///@TODO split this out into a function for meta / event
	if ( cmdchan == 0xFF )  {
		uint8_t cmd ;
		uint8_t size ;

		//xx, nn, dd = command, length, data...
		//skip the command.
		*bytes += fread(&cmd, 1,1, midi->midi_file);
		*bytes += fread(&size, 1,1, midi->midi_file);

		node = malloc((sizeof *node) + size);
		if (node == NULL) {
			midi_set_error((midi_t*)midi, EINVAL, "malloc() failed");
			return NULL;
		}

		fread(node->event.data, size, 1, midi->midi_file);
		*bytes += size;

		node->event.size = size;
		node->event.cmd = cmd;
		node->event.td = td;
		node->event.chan = 0;
		node->event.type = MIDI_TYPE_META;

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
			midi_set_error((midi_t*)midi, EINVAL, "Invalid command, but none running :(.");
			return NULL;
		}

		if (cmd == 12 || cmd == 13)
			argn--;

		node = malloc(sizeof(*node) + argn);
		if (node == NULL) {
			midi_set_error((midi_t*)midi, ENOMEM, "malloc() failed");
			return NULL;
		}

		for ( ; argc < argn; ++argc )
			*bytes += fread(&args[argc], 1, 1, midi->midi_file);

		for (int i = 0; i < argn; ++i)
			node->event.data[i] = args[i];

		node->event.cmd = cmd;
		node->event.td = td;
		node->event.size = (uint8_t)argc;
		node->event.chan = chan;
		node->event.type = MIDI_TYPE_EVENT;

	}

	node->next = NULL;

	return node;
}

static inline uint32_t midi_parse_timedelta(FILE *file, unsigned int *const bytes) {

	uint8_t tmp[4] = {0};
	uint32_t td = 0;

	int read = 0;
	int more;
	do {
		fread(&tmp[read],1,1, file);
		more = tmp[read]&0x80;
		tmp[read] &= 0x7F;
		read++;
	} while (more);

	//need read all the bytes first due to endianness
	for (int i = 0; i < read; ++i)
		td |= tmp[i] << ((read-1-i) * 7);

	*bytes += read;

	return td;
}

void midi_printmeta(midi_event_t * meta) {
	char str[meta->size+1];
	str[meta->size] = '\0';
	strncpy(str, (const char *)meta->data, meta->size);

	printf("%s", str);
}

char *midi_get_eventstr(uint8_t cmd) {
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
	default:
		return "???";
	}
}


void midi_iter_track(midi_track_t *trk) {
	trk->cur = trk->head;
}

bool midi_track_has_next(midi_track_t *trk) {
	return trk->cur != NULL;
}

midi_event_t *midi_track_next(midi_track_t *trk) {
	midi_event_node_t *cur = trk->cur;
	trk->cur = trk->cur->next;
	return &cur->event;
}

static void midi_set_error(midi_t *const midi, const int midi_errno, const char *const errmsg, ...) {
	va_list ap;
	va_start(ap, errmsg);
	vsnprintf(midi->errmsg, sizeof(midi->errmsg), errmsg, ap);
	midi->errnum = midi_errno;
	va_end(ap);
}

static void midi_prefix_errmsg(midi_t *const midi, const char *const errmsg, ...) {
	size_t size = sizeof(midi->errmsg);
	char old_errmsg[size];
	va_list ap;
	int bytes;


	strncpy(old_errmsg, midi->errmsg, size - 1);
	old_errmsg[size - 1] = '\0';
	va_start(ap, errmsg);
	bytes = vsnprintf(midi->errmsg, size, errmsg, ap);
	va_end(ap);

	if ((size_t)bytes < size)
		snprintf(&midi->errmsg[bytes], size - bytes, ": %s", old_errmsg);
}

const char *midi_get_errstr(const midi_t *const midi) {
	return midi->errmsg;
}

int midi_get_errno(const midi_t *const midi) {
	return midi->errnum;
}

static bool midi_check_magic(const uint8_t *const expected, const uint8_t *const check, const size_t magic_size) {
	return memcmp(check, expected, magic_size) == 0;
}
