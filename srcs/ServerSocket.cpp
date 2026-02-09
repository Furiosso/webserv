#include "ServerSocket.hpp"

ServerSocket::ServerSocket(const char* port)
{
	struct addrinfo	hints, *res, *p;
	int				ret;

	ft_bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    //In the line above, check if the macro is correct
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
	ret = getaddrinfo(NULL, port, &hints, &res);
	if (ret != 0)
		std::cerr << gai_strerror(ret);
	for(p = res; p != NULL; p = p->ai_next)
	{
		_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
   		if (_fd != -1)
		{
			int opt = 1;
			if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0)
			{
				if (bind(_fd, p->ai_addr, p->ai_addrlen) == 0)
				{
					if (listen(_fd, SOMAXCONN) == 0)
					{
						int flags;
						flags = fcntl(_fd, F_GETFL);
						flags |= O_NONBLOCK;
						fcntl(_fd, F_SETFL, flags);
						break ;
					}
				}
			}
		}
		close(_fd);
		_fd = -1;
	}
	freeaddrinfo(res);
	if (_fd == -1)
		std::runtime_error("Not valid address found\n");
}

int	ServerSocket::get_fd(){ return _fd; }

ServerSocket::~ServerSocket() {}