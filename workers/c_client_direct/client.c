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

  /* Default vtable. This enables schema objects to be passed through the C API directly to us. */
  Worker_ComponentVtable default_vtable = { 0 };

  /* Generate worker ID. */
  char* worker_id = GenerateWorkerId(argv[3]);

  /* Connect to SpatialOS. */
  Worker_ConnectionParameters params = Worker_DefaultConnectionParameters();
  params.worker_type = "client_direct";
  params.network.tcp.multiplex_level = 4;
  params.default_component_vtable = &default_vtable;
  Worker_ConnectionFuture* connection_future =
	  Worker_ConnectAsync(argv[1], (uint16_t)atoi(argv[2]), worker_id, &params);
  Worker_Connection* connection = Worker_ConnectionFuture_Get(connection_future, NULL);
  Worker_ConnectionFuture_Destroy(connection_future);
  free(worker_id);

  /* Send a test message. */
  Worker_LogMessage message = { WORKER_LOG_LEVEL_WARN, "Client", "Connected successfully", NULL };

  Worker_Connection_SendLogMessage(connection, &message);

  /* Main loop. */
  while (1) {
    if(_kbhit()){
      break;
    }
  }

  Worker_Connection_Destroy(connection);
}
