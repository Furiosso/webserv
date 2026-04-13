#ifndef REQUEST_HANDLER_HPP
# define REQUEST_HANDLER_HPP

# include "ServerSocket.hpp"
# include <algorithm>
# include <sstream>
# include <sys/stat.h>
# include <sys/types.h>
# include <sys/wait.h>

struct HeaderContent
{
	std::string	method;
	size_t		ContentLength;
	bool		isChunked;
	std::string path;
	std::string	protocol;
	std::string	root;
	bool		isAutoindexResponse;
};

struct CgiState
{
    pid_t pid;
    int in_fd;    // parent writes -> child stdin
    int out_fd;   // parent reads  <- child stdout
    std::string write_buf;
    size_t write_pos;
    std::string read_buf;
    bool in_closed;
    bool out_closed;
};

class RequestHandler
{
private:
	Server					_listener;
	int						_fd;
	//int						_status;
	char					_buffer[4096]; // establecer una macro para el tamaño del buffer
	std::string				_request[2];
	int						_error;
	std::string				_header;
	std::string				_body;
	bool					_isHeaderReady;
	bool					_isBodyReady;
	struct HeaderContent	_headerContent;
	struct CgiState			_cgi;
	size_t					_chunkLen;
	std::string				_chunkLine;
	std::string				_sendBuffer;
	bool					_isSent;
public:
	RequestHandler(Server& listener, int fd);
	~RequestHandler();
	void		chargeHeader();
	void		chargeBody();
	void		checkContentLength(size_t num);
	void		chunkManagement();
	void		parseHeader();
	void		setClientFd(int fd);
	int			getClientFd() const;
	bool		getIsHeaderReady() const;
	bool		getIsBodyReady() const;
	Server		getListener() const;
	std::string	getMethod() const;
	void		setPath();
	bool		checkMethod(std::string& method, const std::vector<std::string>& vec);
	void		checkPathValidity(std::string& path, std::vector<std::string>& index, bool autoindex, const std::string& root);
	void		handleCgiIfNeeded();
	std::string	joinPath(const std::string& a, const std::string& b);
	bool		startCgiNonBlocking(const std::string& scriptPath, const std::string& interpreter);
    void		handleCgiFdEvent(int fd, short revents);
    void		finalizeCgiIfDone();
    int			getCgiInFd() const;
    int			getCgiOutFd() const;
    bool		isCgiRunning() const;
	bool		getIsSent() const;
	void		sendResponse();
	std::string	loadContent(const std::string& filename) const;
	void		flushResponse();
	std::string	generateDirectoryListing(const std::string& dirPath, const std::string& requestPath);
	void		handleDelete();

	
};


#endif