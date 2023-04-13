#include "policy_sim.h"

#include <getopt.h>

amr::PolicySimOptions options;

void PrintHelp(int argc, char* argv[]) {
  fprintf(stderr, "\n\tUsage: %s -i <profile_dir>\n", argv[0]);
}

void ParseOptions(int argc, char* argv[]) {
  extern char* optarg;
  extern int optind;
  int c;

  options.prof_dir = "";

  while ((c = getopt(argc, argv, "i:h")) != -1) {
    switch (c) {
      case 'i':
        options.prof_dir = optarg;
        break;
      case 'h':
        PrintHelp(argc, argv);
        exit(0);
    }
  }

  pdlfs::Env* env = pdlfs::Env::Default();
  options.env = env;
  options.nranks = 512;

  if (options.prof_dir == "") {
    logf(LOG_ERRO, "No profile_dir specified!");
    PrintHelp(argc, argv);
    exit(-1);
  }

  if (!options.env->FileExists(options.prof_dir.c_str())) {
    logf(LOG_ERRO, "Directory does not exist!!!");
    PrintHelp(argc, argv);
    exit(-1);
  }

  options.output_dir = options.prof_dir + "/lb_sim";
}

void Run() {
  amr::PolicySim sim(options);
  sim.Run();
}

int main(int argc, char* argv[]) {
  ParseOptions(argc, argv);
  Run();
  return 0;
}
