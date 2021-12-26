
#include "log.h"

#include <signal.h>
#include <stdlib.h>

void signal_stop(int signal) {
  fflush(stdout);
  exit(0);
}

void log_module_init() {
  signal(SIGINT, signal_stop);
}