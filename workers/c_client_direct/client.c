#include <improbable/c_schema.h>
#include <improbable/c_worker.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <conio.h>

#define POSITION_COMPONENT_ID 54
#define LOGIN_COMPONENT_ID 1000
#define CLIENTDATA_COMPONENT_ID 1001

char* GenerateWorkerId(char* worker_id_prefix) {
  /* Calculate buffer size. */
  char* fmt = "%s%d";
  int id = rand() % 10000000;
  int size = snprintf(NULL, 0, fmt, worker_id_prefix, id);

  /* Format string. */
  char* worker_id = malloc(sizeof(char) * (size + 1));
  sprintf(worker_id, fmt, worker_id_prefix, id);
  return worker_id;
}

int main(int argc, char** argv) {
  srand((unsigned int)time(NULL));

  if (argc != 4) {
    printf("Usage: %s <hostname> <port> <worker_id>\n", argv[0]);
    printf("Connects to SpatialOS\n");
    printf("    <hostname>      - hostname of the receptionist to connect to.\n");
    printf("    <port>          - port to use\n");
    printf("    <worker_id>     - name of the worker assigned by SpatialOS. A random prefix will be added to it to ensure uniqueness.\n");
    return EXIT_FAILURE;
  }

  /* Main loop. */
  while (1) {
    if(_kbhit()){
      break;
    }
  }
}
