#include "lib/cli.hxx"

int main(int argc, char **argv) {
  cli::Cli cli(argv);
  return cli.run();
}
