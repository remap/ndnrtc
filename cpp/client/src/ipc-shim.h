// 
// ipc-shim.h
//
// Copyright (c) 2017. Peter Gusev. All rights reserved
//

#ifndef __ipc_shim_h__
#define __ipc_shim_h__

#ifdef __cplusplus
extern "C" {
#endif

// 1-to-many socket with pub protocol
int ipc_setupPubSourceSocket(const char* handle);
// 1-to-many socket with sub protocol
int ipc_setupSubSinkSocket(const char* handle);
// many-to-1 pub socket
int ipc_setupPubSinkSocket(const char* handle);
// many-to-1 sub socket
int ipc_setupSubSourceSocket(const char* handle);

int ipc_shutdownSocket(int socket);

// send data
int ipc_sendData(int socket, void *buffer, size_t bufferLen);
int ipc_sendFrame(int socket, size_t headerSize, const void *headerBuf, void *buffer, size_t bufferLen);

// receive data 
// - buffer will be allocated inside readData
// - client code must free the buffer
int ipc_readData(int socket, void **buffer);
int ipc_readFrame(int socket, void *headerBuf, void *buffer, size_t bufferLen);

const int ipc_lastErrorCode();
const char* ipc_lastError();

#ifdef __cplusplus
}
#endif

#endif