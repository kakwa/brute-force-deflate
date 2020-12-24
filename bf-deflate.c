/* Heavily based on zpipe.c from zlib */

/* zpipe.c: example of proper use of zlib's inflate() and deflate()
   Not copyrighted -- provided to the public domain
   Version 1.4  11 December 2005  Mark Adler */

#include <argp.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <zlib.h>

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#include <fcntl.h>
#include <io.h>
#define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int inf(FILE *source, FILE *dest, long *read) {
  int ret;
  unsigned have;
  z_stream strm;
  unsigned char in[CHUNK];
  unsigned char out[CHUNK];

  /* allocate inflate state */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  // ret = inflateInit(&strm);
  ret = inflateInit2(&strm, -15);
  if (ret != Z_OK)
    return ret;

  /* decompress until deflate stream ends or end of file */
  do {
    strm.avail_in = fread(in, 1, CHUNK, source);
    if (ferror(source)) {
      (void)inflateEnd(&strm);
      return Z_ERRNO;
    }
    if (strm.avail_in == 0)
      break;
    strm.next_in = in;

    /* run inflate() on input until output buffer not full */
    do {
      strm.avail_out = CHUNK;
      strm.next_out = out;
      ret = inflate(&strm, Z_NO_FLUSH);
      // ret = inflateSync(&strm);
      assert(ret != Z_STREAM_ERROR); /* state not clobbered */
      switch (ret) {
      case Z_NEED_DICT:
        ret = Z_DATA_ERROR; /* and fall through */
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
        (void)inflateEnd(&strm);
        return ret;
      }
      have = CHUNK - strm.avail_out;
      if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
        (void)inflateEnd(&strm);
        return Z_ERRNO;
      }
    } while (strm.avail_out == 0);

    /* done when inflate() says it's done */
  } while (ret != Z_STREAM_END);

  if (ret == Z_STREAM_END) {
    *read = strm.total_in;
  }

  /* clean up and return */
  (void)inflateEnd(&strm);
  return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

/* report a zlib or i/o error */
void zerr(int ret) {
  fputs("zpipe: ", stderr);
  switch (ret) {
  case Z_ERRNO:
    if (ferror(stdin))
      fputs("error reading stdin\n", stderr);
    if (ferror(stdout))
      fputs("error writing stdout\n", stderr);
    break;
  case Z_STREAM_ERROR:
    fputs("invalid compression level\n", stderr);
    break;
  case Z_DATA_ERROR:
    fputs("invalid or incomplete deflate data\n", stderr);
    break;
  case Z_MEM_ERROR:
    fputs("out of memory\n", stderr);
    break;
  case Z_VERSION_ERROR:
    fputs("zlib version mismatch!\n", stderr);
  }
}

int infall(FILE *in, char *outdir) {
  long read = 0;
  int ret = 99;
  int ret2 = 0;
  long offset = 0;
  int counter = 0;
  return 0;
  FILE *out = NULL;
  while (ret != Z_OK || ret2 == 0) {
    ret2 = fseek(in, offset, SEEK_SET);
    offset += 1;
    counter += 1;
    ret = inf(in, out, &read);
    // if (ret != Z_OK)
    //    zerr(ret);
    if (ret == Z_OK) {
      fprintf(stderr, "offset       %ld %lX\n", offset, offset);
      fprintf(stderr, "return seek  %d\n", ret2);
      fprintf(stderr, "cur position %ld\n", ftell(in));
      fprintf(stderr, "read  %ld\n", read);
      offset += read;
      counter = 0;
    }
    if (counter == 100) {
      break;
    }
  }
  return ret;
}

const char *argp_program_version = BFD_VERSION;

const char *argp_program_bug_address =
    "Pierre-Francois Carpentier <carpentier.pf@gmail.com>";

static char doc[] = "\nBrute force DEFLATE decompressor";

static struct argp_option options[] = {
    {"input", 'i', "INPUT_FILE", 0, "Input file"},
    {"output", 'o', "OUTPUT_DIR", 0, "Output dir with the decompressed blobs"},
    {0}};

/* A description of the arguments we accept. */
static char args_doc[] = "-i INPUT_FILE -o OUTPUT_DIR";

struct arguments {
  char *args[2]; /* arg1 & arg2 */
  char *output;
  char *input;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = (struct arguments *)state->input;

  switch (key) {
  case 'i':
    arguments->input = arg;
    break;
  case 'o':
    arguments->output = arg;
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};

int is_dir(const char *path) {
  struct stat statbuf;
  if (stat(path, &statbuf) != 0)
    return 0;
  return S_ISDIR(statbuf.st_mode);
}

int main(int argc, char **argv) {
  struct arguments args;
  args.input = NULL;
  args.output = NULL;
  argp_parse(&argp, argc, argv, 0, 0, &args);

  if (args.input == NULL) {
    fprintf(stderr, "error: no -i <input file> arg\n");
    return EXIT_FAILURE;
  }

  if (args.output == NULL) {
    fprintf(stderr, "error: no -o <output dir> arg\n");
    return EXIT_FAILURE;
  }
  FILE *in = fopen(args.input, "r");
  if (in == NULL) {
    fprintf(stderr, "error: failed to open '%s'\n", args.input);
    return EXIT_FAILURE;
  }

  int res = mkdir(args.output, 0700);
  if (res != 0 && !is_dir(args.output)) {
    fprintf(stderr, "error: failed to create output '%s' dir: %s\n",
            args.output, strerror(errno));
    return EXIT_FAILURE;
  } else {
  }
  /* avoid end-of-line conversions */
  // SET_BINARY_MODE(stdin);
  // SET_BINARY_MODE(stdout);

  return infall(in, args.output);
}
