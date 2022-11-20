/*
 * Created on Sun Nov 20 2022
 *
 * Copyright (c) 2022 Philip Zhu Chuyan <me@cyzhu.dev>
 */

#pragma once
#include "cppserv.h"
#include "buffer.h"
#include "socket.h"
#include "cppserv.h"
#include <assert.h>
#include <string>
#include <vector>
#include <functional>

class Context
{
public:
enum State
    {
        INVALID,
        VALID,
        CLOSED
    };
private:
    std::unique_ptr<Socket> __socket;
    std::unique_ptr<Address> __address;
    std::unique_ptr<IOContext> __ioContext; // linked with epoll
    std::unique_ptr<Buffer> __buffer;
    std::unique_ptr<Buffer> __wbuffer;
    std::function<void(int)> __destroyContext;
    std::function<void(Context *)> __onConn;
    std::function<void(Context *)> __onRecv;
    Reactor *__reactor;
    int __socket_fd;
    State __state;
    static const size_t read_buf_size = 1024;
public:
    Context(int cfd, Reactor *reactor, std::function<void(Context *)> onConenct, std::function<void(int)> onDestroy);
    ~Context();
    void handleReadableEvent(); // passed to iocontext
    void flushWriteBuffer(); // passed to iocontext
    void asyncWrite(const void*, size_t);
    ER syncWrite(const void*,size_t);
    ER write(const std::string& data);
    ER write(std::string&&);
    ER write(const std::vector<char> & data);
    ER write(std::vector<char>&&);
    ER writeFile(std::string);
    ER writeFile(std::string, size_t);
    Address& getAddress();
    std::vector<char> read();
    std::string readString();
    State getState();
    void setOnConn(std::function<void(Context *)>);
    void setOnRecv(std::function<void(Context *)>);
    void destory();
};