# SpatialOS C example tutorial

## Dependencies

This project contains workers written in both C and C++ and use [CMake](https://cmake.org/download/)
as their build system. Your system needs to satisfy the
[C++ prerequisites](https://docs.improbable.io/reference/latest/cppsdk/setting-up#prerequisites).
In practice, this just means having a fairly recent compiler that supports C++11 or later.

## Quick start

Build the project and start it with the default launch configuration:

```
spatial worker build --target windows
spatial local launch
```

(Replacing `windows` with `macos` on macOS, or `linux` on Linux).

This will launch SpatialOS locally with a single C++ "physics" worker that updates the position of
a single entity. You may also see a 2nd entity called "physics-worker" created. This entity
represents the managed worker itself.

Note: If you run `spatial worker build` without a `--target` parameter (or with the wrong target
parameter), then the CMake cache for each worker (`workers/<worker>/cmake_build`) may end up in
a corrupt state. To recover, just run `spatial worker clean` to delete the CMake caches.

Now, you can connect either one of the two C client workers (one implemented using "direct"
serialization, the other implemented using "vtable" serialization). These workers can be
launched with the following commands:

* Client (direct): `spatial local worker launch client_direct local`

## Scenario

This project is used to showcase the C API and how it can be used to implement a simple client
worker which visualizes the state of a single entity whose position is updated by a "physics"
worker. As serialization in the C API can be implemented in two different ways, we provide two
implementations of the same worker in `workers/c_client_direct`.
Either one of these can be used as a basis for further experimentation, and the client worker that's
not being used can easily be deleted without breaking any other functionality.

When a client worker connects, it sends a command to the C++ worker (on the `sample.Login`
component). The C++ worker then modifies the entity's write ACLs to delegate component 1001
(`sample.ClientData`) to the client, using the `CallerWorkerAttributes` field of the
`CommandRequestOp`. This causes the entity to be checked out by the client worker, and the client
worker will begin to receive component updates for position changes. The physics worker will also
begin to send a simple command to the client every few seconds.

## Snapshot

The snapshot exists in both text and binary format in the `snapshots` folder. There is no script
to generate the snapshot as the snapshot was written by hand in the text format, but it's possible
to make simple changes to the text format and regenerate the binary snapshot from it. To update the
binary snapshot after making a change, run the following command:

```
spatial project history snapshot convert --input-format=text --input=snapshots/default.txt --output-format=binary --output=snapshots/default.snapshot
```

## Tutorial steps

1. introduction
    * Describe the tutorial project scenario
        * A C++ worker that creates an entity and moves it around
        * Our goal is to write a new worker using the C api
        * Our new worker will connect, start sending a command to the C++ worker
        * Start receiving the updates on the entity
        * Will only run locally for the purpose of this presentation, but possible to run in the cloud with ease
1. Starting point

    Checkout the tutorial starting point branch
    ```
    git clone git@github.com:improbable/PEng-EIE-CAPI-Onsite.git -b tutorial-starting-point
    ```

1. Review project layout
    * A note on SPL vs FPL
        Mention FPL benefits for engine integrations
    * Dependencies
    * C++ worker
    * Schema
    * Snapshots
    * The C worker stub
        * Setting up the dependencies

            The worker package file is empty at this point
        * Build.json

            No build step present
        * spatialos.client_direct.worker.json

            Just review the setup
        * CMakeLists.txt

            Just standard CMake fu, no need to change anything
        * Client.c

            Should just be an empty main file
1. Start a local deployment
    * Introduce the default_launch.json config
    * Build the project
        spatial local launch
        Observe the C++ worker and an entity without any name moving around
    * Go through the inspector

1. Setup connection
    * Add to the beginning of the main file

    ```cpp

    /* Default vtable. This enables schema objects to be passed through the C API directly to us. */
    Worker_ComponentVtable default_vtable = {0};

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
    Worker_LogMessage message = {WORKER_LOG_LEVEL_WARN, "Client", "Connected successfully", NULL};

    Worker_Connection_SendLogMessage(connection, &message);

    ```

   * Destroy the connection at the end of the file

    ```cpp
    Worker_Connection_Destroy(connection);
    ```

    **NOTE:** If successful, you should see the connection output in the message from the worker and the local deployment logs and will shut down when a key is pressed

    ```
    spatial local worker launch client_local local
    ```

1. Setup basic dispatching and process logging and disconnection
    
    * Add the op handlers

    ```cpp
    /** Op handler functions. */
    void OnLogMessage(const Worker_LogMessageOp* op) {
    printf("log: %s\n", op->message);
    }

    void OnDisconnect(const Worker_DisconnectOp* op) {
    printf("disconnected. reason: %s\n", op->reason);
    }
    ```

    * Setup the dispatcher

    ```cpp
    Worker_OpList* op_list = Worker_Connection_GetOpList(connection, 0);
    for (size_t i = 0; i < op_list->op_count; ++i) {
    Worker_Op* op = &op_list->ops[i];
    switch (op->op_type) {
    case WORKER_OP_TYPE_DISCONNECT:
      OnDisconnect(&op->op.disconnect);
      break;
    case WORKER_OP_TYPE_LOG_MESSAGE:
       OnLogMessage(&op->op.log_message);
       break;
     default:
       break;
       }
    }
    Worker_OpList_Destroy(op_list);
    ```

    **Note:** move the _kbhit call at this point. If you turn off the local deployment while running the connected worker and press a key in the terminal running your client. If things are setup correctly you should see our log messages.

    **Note:** the completed state is available in the branch tutorial-setup-dispatcher for reference

1. Setup entity query
   
   * Add entity query handler next to the log and disconnect handlers

    ```cpp
    void OnEntityQueryResponse(const Worker_EntityQueryResponseOp* op) {
        printf("entity query result: %d entities. Status: %d. Results: %p\n", op->result_count,
                op->status_code, (void*)op->results);
        if (op->results) {
        for (uint32_t i = 0; i < op->result_count; ++i) {
            const Worker_Entity* entity = &op->results[i];
            printf("- entity %" PRId64 " with %d components", entity->entity_id, entity->component_count);
            for (uint32_t k = 0; k < entity->component_count; ++k) {
            if (entity->components[k].component_id == POSITION_COMPONENT_ID) {
                Schema_Object* coords_object =
                    Schema_GetObject(Schema_GetComponentDataFields(entity->components[k].schema_type), 1);
                double x = Schema_GetDouble(coords_object, 1);
                double y = Schema_GetDouble(coords_object, 2);
                double z = Schema_GetDouble(coords_object, 3);
                printf(": Position: (%f, %f, %f)\n", x, y, z);
            }
            }
            if (entity->component_count == 0) {
            printf("\n");
            }
        }
        }
    }
    ```

    * Add the handler to the dispatcher

    ```cpp
    case WORKER_OP_TYPE_ENTITY_QUERY_RESPONSE:
       OnEntityQueryResponse(&op->op.entity_query_response);
       break;
    ```

    * Setup the entity query

    ```cpp
     /* Send an entity query. */
    Worker_EntityQuery query;
    query.constraint.constraint_type = WORKER_CONSTRAINT_TYPE_ENTITY_ID;
    query.constraint.constraint.entity_id_constraint.entity_id = 1;
    query.result_type = WORKER_RESULT_TYPE_SNAPSHOT;
    query.snapshot_result_type_component_id_count = 1;
    Worker_ComponentId position_component_id = POSITION_COMPONENT_ID;
    query.snapshot_result_type_component_ids = &position_component_id;
    Worker_Connection_SendEntityQueryRequest(connection, &query, NULL);
    ```

    **Note:** the completed state is available in the branch tutorial-setup-entity-query for reference

1. Send login command

    * Add the command handler function

    ```cpp
    void OnCommandRequest(Worker_Connection* connection, const Worker_CommandRequestOp* op) {
        Schema_FieldId command_index = op->request.command_index;
        printf("received command request (entity: %" PRId64 ", component: %d, command: %d).\n",
                op->entity_id, op->request.component_id, command_index);

        if (op->request.component_id == CLIENTDATA_COMPONENT_ID && command_index == 1) {
            Schema_Object* payload = Schema_GetCommandRequestObject(op->request.schema_type);
            int payload1 = Schema_GetInt32(payload, 1);
            float payload2 = Schema_GetFloat(payload, 2);

            float sum = payload1 + payload2;
            Worker_CommandResponse response = {0};
            response.command_index = command_index;
            response.component_id = op->request.component_id;
            response.schema_type = Schema_CreateCommandResponse();
            Schema_Object* response_object = Schema_GetCommandResponseObject(response.schema_type);
            Schema_AddFloat(response_object, 1, sum);
            Worker_Connection_SendCommandResponse(connection, op->request_id, &response);

            printf("sending command response. Sum: %f\n", sum);
        }
    }
    ```

    * Add the command handler to the dispatcher

    ```cpp
    case WORKER_OP_TYPE_COMMAND_REQUEST:
       OnCommandRequest(connection, &op->op.command_request);
       break;
    ```

    * Send the command

    ```cpp
    /* Take control of the entity. */
    Worker_CommandRequest command_request;
    memset(&command_request, 0, sizeof(command_request));
    command_request.component_id = LOGIN_COMPONENT_ID;
    command_request.command_index = 1;
    command_request.schema_type = Schema_CreateCommandRequest();
    Worker_CommandParameters command_parameters;
    command_parameters.allow_short_circuit = 0;
    Worker_Connection_SendCommandRequest(connection, 1, &command_request, NULL, &command_parameters);
    ```

    **Note:** the completed state is available in the branch tutorial-setup-login-command for reference

1. Add processing of receiving add component op

    * Add the handler

    ```cpp
    void OnAddComponent(const Worker_AddComponentOp* op) {
        printf("received add component op (entity: %" PRId64 ", component: %d)\n", op->entity_id,
                op->data.component_id);

        if (op->data.component_id == POSITION_COMPONENT_ID) {
        /* Received position data */
        double x, y, z;
        Schema_Object* coords_object =
            Schema_GetObject(Schema_GetComponentDataFields(op->data.schema_type), 1);
        x = Schema_GetDouble(coords_object, 1);
        y = Schema_GetDouble(coords_object, 2);
        z = Schema_GetDouble(coords_object, 3);
        printf("received improbable.Position initial data: (%f, %f, %f)\n", x, y, z);
        }
    }
    ```

    **Note:** the completed state is available in the branch tutorial-setup-add-component-op-processing for reference

1. Add processing of receiving component updates

    * Add the handler function 

    ```cpp
    void OnComponentUpdate(const Worker_ComponentUpdateOp* op) {
    printf("received component update op (entity: %" PRId64 ", component: %d)\n", op->entity_id,
            op->update.component_id);

    if (op->update.component_id == POSITION_COMPONENT_ID) {
    /* Received position update */
    Schema_Object* coords_object =
        Schema_GetObject(Schema_GetComponentUpdateFields(op->update.schema_type), 1);
    double x = Schema_GetDouble(coords_object, 1);
    double y = Schema_GetDouble(coords_object, 2);
    double z = Schema_GetDouble(coords_object, 3);
    printf("received improbable.Position update: (%f, %f, %f)\n", x, y, z);
    }
    }
    ```

    * Add the handler to the dispatcher

    ```cpp
    case WORKER_OP_TYPE_COMPONENT_UPDATE:
       OnComponentUpdate(&op->op.component_update);
       break;
    ```

    **Note:** the completed state is available in the branch tutorial-complete for reference

1. Observe and celebrate