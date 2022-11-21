/*
 * Created on Sun Nov 20 2022
 *
 * Copyright (c) 2022 Philip Zhu Chuyan <me@cyzhu.dev>
 */

#include "context.hpp"

Context::Context(int cfd, Address address, Reactor *reactor,
                 std::function<void(Context *)> onConenct, std::function<void(int)> onDestroy) : __socket(std::make_unique<Socket>(cfd)),
                                                                                                 __buffer(std::make_unique<Buffer>()),
                                                                                                 __wbuffer(std::make_unique<Buffer>()),
                                                                                                 __reactor(reactor),
                                                                                                 __destroyContext(onDestroy),
                                                                                                 __onConn(std::move(onConenct)),
                                                                                                 __address(std::move(address))
{
    assert(__destroyContext);
    __ioContext = std::make_unique<IOContext>(__reactor, cfd);
    __ioContext->enableRead(); // register ioContext on reactor and therefore epoll
    __ioContext->enableWrite();
    __ioContext->setET();
    __state = VALID;
    if(__onConn) __onConn(this);
}

Context::~Context() {}

void Context::flushWriteBuffer()
{
    size_t bytes_written;
    while (__wbuffer->size() > 0)
    {
        bytes_written = ::write(__socket_fd, __wbuffer->getReader(), __wbuffer->size());
        if (bytes_written > 0)
        {
            __wbuffer->moveReadCursor(bytes_written);
        }
        else if (bytes_written == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
        {
            break;
        }
        else if (bytes_written == 0)
        {
            __state = CLOSED;
            printf("read EOF, client fd %d disconnected\n", __socket_fd);
            this->destory();
            break;
        }
        else
        {
            __state = INVALID;
            printf("Unknown error on fd=%d, socket closed and context destroyed\n", __socket_fd);
            this->destory();
            break;
        }
    }
}

void Context::handleReadableEvent()
{
    int fd = __socket->getFd();
    size_t bytes_read;
    while (1)
    {
        void *writer = __buffer->getWriter(1024);
        bytes_read = ::read(fd, writer, 1024);
        if (bytes_read > 0)
        {
            if (Context::read_buf_size - bytes_read > 0)
                __buffer->shrink(Context::read_buf_size - bytes_read);
        }
        else if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            break;
        }
        else if (bytes_read == 0)
        {
            __state = CLOSED;
            printf("read EOF, client fd %d disconnected\n", fd);
            this->destory();
            break;
        }
        else
        {
            __state = INVALID;
            printf("Unknown error on fd=%d, socket closed and context destroyed\n", fd);
            this->destory();
            break;
        }
    }
    if(__onRecv) __onRecv(this);
}

void Context::asyncWrite(const void *buf, size_t size)
{
    __wbuffer->put(buf, size);
    flushWriteBuffer();
}

ER Context::syncWrite(const void *buf, size_t size)
{
    size_t bytes_written;
    size_t remaining = size;
    while (remaining > 0)
    {
        bytes_written = ::write(__socket_fd, buf, remaining);
        if (bytes_written > 0)
        {
            remaining -= bytes_written;
        }
        else if (bytes_written == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
        {
            if (remaining > 0)
                asyncWrite((const char*)buf + (size - remaining), remaining);
            return ER::SUCCESS;
        }
        else if (bytes_written == 0)
        {
            __state = CLOSED;
            printf("read EOF, client fd %d disconnected\n", __socket_fd);
            this->destory();
            return ER::SOCKET_ERROR;
        }
        else
        {
            __state = INVALID;
            printf("Unknown error on fd=%d, socket closed and context destroyed\n", __socket_fd);
            this->destory();
            return ER::SOCKET_ERROR;
        }
    }
    return ER::SUCCESS;
}

ER Context::write(const std::string &data)
{
    return syncWrite(data.c_str(), data.size() + 1);
}

ER Context::write(std::string &&data)
{
    return write(std::move(data));
}

ER Context::write(const std::vector<char> &data)
{
    return syncWrite(data.data(), data.size());
}

ER Context::write(std::vector<char> &&data)
{
    return write(std::move(data));
}

ER Context::writeFile(std::string filePath)
{
    FILE *file;
    size_t bytes_read;
    if (file = ::fopen(filePath.c_str(), "r"))
    {
        while (1)
        {
            bytes_read = fread(__wbuffer->getWriter(Context::read_buf_size), sizeof(char), Context::read_buf_size, file);
            if (bytes_read < Context::read_buf_size)
            {
                __wbuffer->shrink(Context::read_buf_size - bytes_read);
            }
            if (bytes_read == 0)
            {
                break;
            }
        }
        flushWriteBuffer();
        return ER::SUCCESS;
    }
    else
    {
        return ER::FILE_ERROR;
    }
}

ER Context::writeFile(std::string filePath, size_t size)
{
    return ER::UNIMPLEMENTED;
}

const Address &Context::getAddress()
{
    return __address;
}

std::vector<char> Context::read()
{
    return std::move(__buffer->get());
}

std::string Context::readString()
{
    auto v = __buffer->getInnerVec();
    std::string ret(v.begin(), v.end());
    __buffer->clear();
    return std::move(ret);
}

Context::State Context::getState()
{
    return __state;
}

void Context::setOnRecv(std::function<void(Context *)> onRecv)
{
    __onRecv = std::move(onRecv);
}

void Context::destory()
{
    __destroyContext(__socket_fd);
}

// ~Context();
// std::vector<char> read();
// std::string readString();
// ER write(std::string);
// ER write(std::vector<char>);
// ER writeFile(std::string);
// ER writeFile(std::string, size_t);
// State getState();
// void setOnConn(std::function<void(Context*)>);
// void setOnRecv(std::function<void(Context*)>);
// void handleReadableEvent(); // passed to iocontext
// void destory();