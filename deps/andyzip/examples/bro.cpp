////////////////////////////////////////////////////////////////////////////////
//
// Brotli utility example
//
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <andyzip/brotli_decoder.hpp>

#include <iostream>
#include <fstream>
#include <chrono>

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
  #include <sys/stat.h>
  #include <io.h>
  #include <fcntl.h>
#else
#endif

class bro {
  struct args_t {
    bool force = false;
    bool verbose = false;
    bool decompress = false;
    bool no_copy_stat = false;
    const char *input_file = nullptr;
    const char *output_file = nullptr;
    const char *custom_dictionary = nullptr;
    int quality = 0;
    int repeat = 1;
    int window = 22;
    bool fail = true;

    void error(const char *arg, const char *message, const char **switches) {
      fprintf(stderr, "%s \"%s\"\n", message, arg);
      fprintf(stderr,
        "Usage: bro [--force] [--quality n] [--decompress]"
        " [--input filename] [--output filename] [--repeat iters]"
        " [--verbose] [--window n] [--custom-dictionary filename]"
        " [--no-copy-stat]\n"
      );
    }

    args_t(int argc, const char **argv) {
      static const char *switches[] = {
        "f", "-force",
        "d", "-decompress", "-uncompress",
        "v", "-verbose",
        "N", "-no-copy-stat",
        "i", "-in", "-input",
        "o", "-out", "-output"
        "D", "-custom-dictionary",
        "q", "-quality",
        "r", "-repeat",
        "w", "-window",
      };

      for (int i = 1; i != argc; ++i) {
        const char *arg = argv[i];
        if (arg[0] == '-') {
          char short_code = '\0';
          for (auto sw : switches) {
            if (!sw[1]) short_code = sw[0];
            if (sw[0] == arg[1] && !strcmp(sw+1, arg+2)) {
              switch(short_code) {
                case 'f': {
                  force = true;
                } break;
                case 'd': case 'u': {
                  decompress = true;
                } break;
                case 'v': {
                  verbose = true;
                } break;
                case 'N': {
                  if (!sw[1]) { error(arg, "unknown argument", switches); return; }
                  no_copy_stat = true;
                } break;
                case 'i': case 'o': {
                  if (i+1 == argc) { error(arg, "too few arguments", switches); return; }
                  if (short_code == 'D' && !sw[1]) { error(arg, "unknown argument", switches); return; }
                  const char * &rhs = short_code == 'i' ? input_file : 'o' ? output_file : custom_dictionary;
                  rhs = argv[++i];
                } break;
                case 'q': case 'r': case 'w': {
                  if (i+1 == argc) { error(arg, "too few arguments", switches); return; }
                  int &rhs = short_code == 'q' ? quality : short_code == 'r' ? repeat : window;
                  rhs = 0;
                  const char *p = argv[++i];
                  while (*p) {
                    if (*p < '0' || *p > '9') { error(p, "expected number", switches); return; }
                    rhs = rhs * 10 + *p - '0';
                  }
                } break;
                default: {
                  error(arg, "unknown argument", switches); return;
                } break;
              }
              break;
            }
          }
        } else {
          error(arg, "unexpected argument", switches);
          return;
        }
      }

      if (quality > 99) {
        error("", "quality out of range", switches);
      } else if (window < 10 || window >= 25) {
        error("", "window out of range", switches);
      } else {
        fail = false;
      }
    }
  };

public:
  bro(int argc, const char **argv) {
    args_t args(argc, argv);
    if (args.fail) return;

    #ifdef MSC_VER
      _setmode(_fileno(stdin), _O_BINARY);
      _setmode(_fileno(stdout), _O_BINARY);
    #endif

    std::ifstream ifs;
    if (args.input_file) {
      ifs.open(args.input_file, std::ios::binary);
    }

    std::istream &is = args.input_file ? static_cast<std::istream&>(ifs) : std::cin;
    
    //std::ostream os = !args.output_file ? std::cout : (std::ostream&)std::ofstream(args.output_file, std::ios::binary);

    andyzip::brotli_decoder dec;

    andyzip::brotli_decoder_state state;

    static char tmp[0x1000000];
    if (!ifs.bad() && !ifs.eof()) {
      auto start = std::chrono::high_resolution_clock::now();
      ifs.read(tmp, sizeof(tmp));
      state.src = tmp;
      state.bitptr = 0;
      state.bitptr_max = (std::uint32_t)(ifs.gcount() * 8);
      state.log_file = fopen("log.txt", "wb");
      dec.decode(state);
      auto end = std::chrono::high_resolution_clock::now();
      auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
      printf("err=%d\n", (int)state.error);
      printf("%d\n", (int)state.bytes_written);
      if (args.verbose) {
        printf("Brotli decompression speed: %0.f MB/s\n", (1000000000.0/1024/1024) * state.bytes_written / nanoseconds.count());
      }
    }
  }
};

int main(int argc, const char **argv) {
  bro(argc, argv);
}

