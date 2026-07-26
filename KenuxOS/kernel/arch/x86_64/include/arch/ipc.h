#ifndef ARCH_X86_64_IPC_H
#define ARCH_X86_64_IPC_H

#include <arch/types.h>

#define IPC_MAX_MESSAGES 64
#define IPC_MAX_SIZE 256

typedef struct {
    uint64_t source;
    uint64_t destination;
    uint64_t type;
    uint64_t size;
    uint8_t data[IPC_MAX_SIZE];
} ipc_message_t;

void ipc_init(void);
int ipc_send(uint64_t destination, uint64_t type, const void* data, uint64_t size);
int ipc_receive(uint64_t source, uint64_t* type, void* data, uint64_t* size);

#endif
