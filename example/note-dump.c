#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/mman.h>

#include <midi-parser.h>

static void usage(const char *prog)
{
  printf("usage: %s <file.midi>\n", prog);
}

static void parse_and_dump(struct midi_parser *parser)
{
  enum midi_parser_status status;

  while (1) {
    status = midi_parse(parser);
    switch (status) {
    case MIDI_PARSER_EOB:
      puts("};");
      return;

    case MIDI_PARSER_ERROR:
      puts("error");
      return;

    case MIDI_PARSER_INIT:
      break;

    case MIDI_PARSER_HEADER:
      puts("{");
      break;

    case MIDI_PARSER_TRACK:
      break;

    case MIDI_PARSER_TRACK_MIDI:
      puts("track-midi");
      printf("  time: %ld\n", parser->vtime);
      printf("  status: %d [%s]\n", parser->midi.status, midi_status_name(parser->midi.status));
      printf("  channel: %d\n", parser->midi.channel);
      printf("  param1: %d\n", parser->midi.param1);
      printf("  param2: %d\n", parser->midi.param2);
      break;

    case MIDI_PARSER_TRACK_META:
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

#ifdef _WIN32

  HANDLE fhandle = (HANDLE)_get_osfhandle(fd);

  if (st.st_size == 0) {
    printf("file is empty\n");
    close(fd);
    return 1;
  }

  HANDLE hMapFile = CreateFileMapping(fhandle, NULL, PAGE_READONLY, 0, 0, NULL);

  if (!hMapFile) {
    win_err_helper("CreateFileMapping", path);
    close(fd);
    return 1;
  }

  void *mem = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);

  if (!mem) {
    win_err_helper("MapViewOfFile", path);
    CloseHandle(hMapFile);
    close(fd);
    return 1;
  }

#else

  void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED) {
    printf("mmap(%s): %m\n", path);
    close(fd);
    return 1;
  }

#endif

  struct midi_parser parser;
  parser.state = MIDI_PARSER_INIT;
  parser.size  = st.st_size;
  parser.in    = mem;

  parse_and_dump(&parser);

#ifdef _WIN32
  UnmapViewOfFile(mem);
  CloseHandle(hMapFile);
#else
  munmap(mem, st.st_size);
#endif
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
