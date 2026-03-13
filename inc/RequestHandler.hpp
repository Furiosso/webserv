#ifndef REQUEST_HANDLER_HPP
# define REQUEST_HANDLER_HPP

# include "ServerSocket.hpp"
# include <algorithm>
# include <sstream>
# include <sys/stat.h>

struct HeaderContent
{
	std::string	method;
	size_t		contentLenght;
	bool		isChunked;
	std::string path;
	std::string	protocol;
};

class RequestHandler
{
private:
	Server					_listener;
	int						_fd;
	char					_buffer[4096]; // establecer una macro para el tamaño del buffer
	std::string				_request[2];
	int						_error;
	std::string				_header;
	std::string				_body;
	struct HeaderContent	_headerContent;
public:
	RequestHandler(Server& listener, int fd);
	~RequestHandler();
	void	chargeHeader(int fd, size_t maxBodySize);
	void	parseHeader();
	void	setClientFd(int fd);
	int		getClientFd() const;
	void	setPath();
	bool	checkMethod(std::string& method, const std::vector<std::string>& vec);
	void	checkPathValidity(std::string& path, std::vector<std::string>& index, bool autoindex, const std::string& root);
	std::string	joinPath(const std::string& a, const std::string& b);
};


#endif