//
// ipc-shim.c
//
// Copyright (c) 2017. Peter Gusev. All rights reserved
//

#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "ipc-shim.h"

#define FRAME_HEADER_SIZE 512

int ipc_setupSocket(const char *handle, int isPub, int isBind)
{
    int socket = nn_socket(AF_SP, (isPub == 1 ? NN_PUB : NN_SUB));

    if (isPub != 1) // set receive all messages is SUB
        nn_setsockopt(socket, NN_SUB, NN_SUB_SUBSCRIBE, "", 0);

    if (socket >= 0)
    {
        char str[256];
        memset(str, 0, 256);
        sprintf(str, "ipc://%s", handle);

        int endpoint = (isBind ? nn_bind(socket, str) : nn_connect(socket, str));
        if (endpoint < 0)
            return -1;
    }
    return socket;
}

int ipc_shutdownSocket(int socket)
{
    return nn_close(socket);
}

int ipc_setupPubSourceSocket(const char *handle)
{
    return ipc_setupSocket(handle, 1, 1);
}

int ipc_setupSubSinkSocket(const char *handle)
{
    return ipc_setupSocket(handle, 0, 0);
}

int ipc_setupPubSinkSocket(const char *handle)
{
    return ipc_setupSocket(handle, 1, 0);
}

int ipc_setupSubSourceSocket(const char *handle)
{
    return ipc_setupSocket(handle, 0, 1);
}

int ipc_sendData(int socket, void *buffer, size_t bufferLen)
{
    struct nn_msghdr msgHdr;
    struct nn_iovec iov;

    void *b = nn_allocmsg(bufferLen, 0); // it will be deallocated automatically upon successfull send
    memcpy(b, buffer, bufferLen);

    iov.iov_base = &b; // this is curious (pointer of a pointer)
    iov.iov_len = NN_MSG;
    memset(&msgHdr, 0, sizeof(msgHdr));
    msgHdr.msg_iov = &iov;
    msgHdr.msg_iovlen = 1;

    int ret = nn_sendmsg(socket, &msgHdr, NN_DONTWAIT);

    if (ret < 0)
        free(b);

    return ret;
}

int ipc_sendFrame(int socket,
                  size_t headerSize, const void *headerBuf,
                  void *buffer, size_t bufferLen)
{
    static unsigned char HeaderBuf[FRAME_HEADER_SIZE];
    memset(HeaderBuf, 0, FRAME_HEADER_SIZE);

    size_t copyLen = (headerSize < FRAME_HEADER_SIZE ? headerSize : FRAME_HEADER_SIZE);
    memcpy((void*)HeaderBuf, headerBuf, copyLen);

    struct nn_msghdr msgHdr;
    struct nn_iovec iov[2];

    iov[0].iov_base = HeaderBuf;
    iov[0].iov_len = FRAME_HEADER_SIZE;
    iov[1].iov_base = buffer;
    iov[1].iov_len = bufferLen;
    memset(&msgHdr, 0, sizeof(msgHdr));
    msgHdr.msg_iov = iov;
    msgHdr.msg_iovlen = 2;

    return nn_sendmsg(socket, &msgHdr, NN_DONTWAIT);
}

int ipc_readData(int socket, void **buffer)
{
    struct nn_msghdr msgHdr;
    struct nn_iovec iov;

    void *b;

    iov.iov_base = &b;
    iov.iov_len = NN_MSG;
    memset(&msgHdr, 0, sizeof(msgHdr));
    msgHdr.msg_iov = &iov;
    msgHdr.msg_iovlen = 1;

    int ret = nn_recvmsg(socket, &msgHdr, 0);

    if (ret > 0)
    {
        *buffer = malloc(ret);
        memcpy(*buffer, b, ret);
        nn_freemsg(b);
    }

    return ret;
}

int ipc_readFrame(int socket, void *headerBuf, void *buffer, size_t bufferLen)
{
    struct nn_msghdr msgHdr;
    struct nn_iovec iov[2];

    iov[0].iov_base = headerBuf;
    iov[0].iov_len = FRAME_HEADER_SIZE;
    iov[1].iov_base = buffer;
    iov[1].iov_len = bufferLen;
    memset(&msgHdr, 0, sizeof(msgHdr));
    msgHdr.msg_iov = iov;
    msgHdr.msg_iovlen = 2;

    return nn_recvmsg(socket, &msgHdr, 0);
}

const int ipc_lastErrorCode()
{
    return nn_errno();
}

const char *ipc_lastError()
{
    return nn_strerror(nn_errno());
}
