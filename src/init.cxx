#include "lib/cli.hxx"

int main(int argc, char **argv) {
  cli::Cli cli(argc, argv);
  return cli.run();
}
