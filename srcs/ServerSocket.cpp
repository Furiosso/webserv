#include "ServerSocket.hpp"

ServerSocket::ServerSocket(const char* port)
{
	struct addrinfo	hints, *res, *p;

	getaddrinfo(NULL, port, &hints, &res);
	for(p = res; p != NULL; p = p->ai_next)
	{
		_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
   		if (_fd == -1)
		{
			std::cout << "Error creating socket: " << strerror(errno) << "\n";
			continue ;
		}
		if (bind(_fd, p->ai_addr, p->ai_addrlen) == 0)
		{
			listen(_fd, SOMAXCONN);
			break ;
		}
		close(_fd);
	}
	freeaddrinfo(res);
}