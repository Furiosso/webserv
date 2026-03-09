#include "RequestHandler.hpp"

RequestHandler::RequestHandler(Server& listener, int fd) : _listener(listener), _fd(fd)
{
}

RequestHandler::~RequestHandler()
{
}

void RequestHandler::setClientFd(int fd)
{
	this->_fd = fd;
}