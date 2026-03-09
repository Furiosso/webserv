#ifndef REQUEST_HANDLER_HPP
# define REQUEST_HANDLER_HPP

# include "ServerSocket.hpp"

class RequestHandler
{
private:
	int				_fd;
	Server			_listener;
public:
	RequestHandler(Server& listener, int fd);
	~RequestHandler();
	void	setClientFd(int fd);
};


#endif