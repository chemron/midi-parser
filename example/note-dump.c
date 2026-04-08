#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>

#include <sys/mman.h>

#include <midi-parser.h>

long tempo = 500000;   // default = 120 BPM
int division = 480;    // will be set from header

static void usage(const char *prog)
{
  printf("usage: %s <file.midi>\n", prog);
}

int get_highest_note(int active_notes[128]) {
  int i;
  for (i = 127; i >=0; i--) {
    if (active_notes[i]) {
        return i;
    }
  }
  return -1;
}

int get_duration_ms(long ticks) {
    long ms = (ticks * tempo) / division;
    return (int)(ms / 1000);
}

int get_midi_freq(int note) {
    return (int)(440.0 * pow(2.0, (note - 69) / 12.0));
}

static void parse_and_dump(struct midi_parser *parser)
{
  enum midi_parser_status status;
  int active_notes[128] = {0};
  int current_note = -1;
  int new_note = -1;
  long duration;
  long current_time = 0;
  long last_time = 0;

  int vtime;
  int midi_status;
  int channel;
  int note;
  int vel;

  while (1) {
    status = midi_parse(parser);
    switch (status) {
    case MIDI_PARSER_ERROR:
      puts("error");
      return;

    case MIDI_PARSER_INIT:
      break;

    case MIDI_PARSER_HEADER:
      puts("#include \"music.h\"\n\nstruct note_t music[] = {");
      division = parser->header.time_division;
      break;

    case MIDI_PARSER_TRACK:
      break;

    case MIDI_PARSER_TRACK_MIDI:
      vtime = parser->vtime;
      midi_status = parser->midi.status;
      channel = parser->midi.channel;
      note = parser->midi.param1;
      vel = parser->midi.param2;

      current_time += vtime;

      if (channel == 9) {
        break; // skip drums
      }

      if (midi_status == MIDI_STATUS_NOTE_ON) {
        if (vel > 0) {
          active_notes[note] = 1;
        }
        else {
          active_notes[note] = 0;
        }
      }
      else if (midi_status == MIDI_STATUS_NOTE_OFF) {
        active_notes[note] = 0;
      }

      new_note = get_highest_note(active_notes);

      if (new_note != current_note) {
          duration = current_time - last_time;

          if (duration > 0) {
              int duration_ms = get_duration_ms(duration);

              if (duration_ms >= 20) {
                  if (current_note != -1) {
                      int freq = get_midi_freq(current_note);
                      printf("{%d, %d},\n", freq, duration_ms);
                  } else {
                      printf("{0, %d},\n", duration_ms);
                  }
              }
          }

          last_time = current_time;
          current_note = new_note;
      }
      break;

    case MIDI_PARSER_EOB:
      duration = current_time - last_time;
      if (duration > 0) {
        int duration_ms = get_duration_ms(duration);
        if (current_note != -1) {
          int freq = get_midi_freq(current_note);
          printf("{%d, %d},\n", freq, duration_ms);
        }
        else {
            printf("{0, %d},\n", duration_ms);
        }
      }
      puts("};\nconst int music_length = sizeof(music) / sizeof(struct note_t);");
      return;


    case MIDI_PARSER_TRACK_META:
      if (parser->meta.type == MIDI_META_SET_TEMPO) {
        tempo =
            (parser->meta.bytes[0] << 16) |
            (parser->meta.bytes[1] << 8)  |
            (parser->meta.bytes[2]);
      }
      break;

    case MIDI_PARSER_TRACK_SYSEX:
      break;

    default:
      printf("unhandled state: %d\n", status);
      return;
    }
  }
}

static int parse_file(const char *path)
{
  struct stat st;

  if (stat(path, &st)) {
    printf("stat(%s): %m\n", path);
    return 1;
  }

  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    printf("open(%s): %m\n", path);
    return 1;
  }

  void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED) {
    printf("mmap(%s): %m\n", path);
    close(fd);
    return 1;
  }

  struct midi_parser parser;
  parser.state = MIDI_PARSER_INIT;
  parser.size  = st.st_size;
  parser.in    = mem;

  parse_and_dump(&parser);
  munmap(mem, st.st_size);

  close(fd);
  return 0;
}

int main(int argc, char **argv)
{
  if (argc != 2) {
    usage(argv[0]);
    return 1;
  }

  return parse_file(argv[1]);
}
