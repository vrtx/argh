#include "argh.h"
// #include <iostream>

using namespace std;

struct Options {
  const char* infile;
  const char* tmppath;
  const char* outfile;
  float rate;
  bool debug;
  bool verbose;
} gOptions;

int main(int argc, const char* argv[]) {
    argh::Args a(argc, argv);
    a.arg(&gOptions.infile,  'i', "input",   "Specify the input file",   "./in.foo");
    a.arg(&gOptions.tmppath, 't', "temp",    "Path for temporary files", "/tmp/");
    a.arg(&gOptions.rate,    'r', "rate",    "Rate of enytopy",          0.75f);
    a.arg(&gOptions.debug,   'd', "debug",   "Start in daemon mode");
    a.arg(&gOptions.verbose, 'v', "verbose", "Level of verbosity");
    a.remainder("output path");
    if (!a.parse()) {
        // parse failed
         cout << a.parser_.errors() << endl;
         cout << a.help() << endl;
    }
    std::cout << "Rate: " << gOptions.rate << std::endl;
    // gOptions is now set to command line values or defaults values. Checks for
    // required/conflicting arguments or value ranges should generally be done here.
    return 0;
}
