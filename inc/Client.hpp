#ifndef CLIENT_HPP
# define CLIENT_HPP

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
	std::string requestUri; // raw request URI (path + optional ?query)
	std::string	protocol;
	std::string	root;
	std::string	host;
	bool		isAutoindexResponse;
};

struct CgiState
{
    pid_t		pid;
    int			in_fd;    // parent writes -> child stdin
    int			out_fd;   // parent reads  <- child stdout
    std::string	write_buf;
    size_t		write_pos;
    std::string	read_buf;
    bool		in_closed;
    bool		out_closed;
	bool		finalized;
};

class Client
{
private:
	Server					_listener;
	int						_fd;
	char					_buffer[4096]; // establecer una macro para el tamaño del buffer
	std::string				_request[2];
	int						_status;
	std::string				_header;
	std::string				_body;
	bool					_isHeaderReady;
	bool					_isBodyReady;
	struct HeaderContent	_headerContent;
	struct CgiState			_cgi;
	size_t					_chunkLen;
	std::string				_chunkLine;
	std::string				_sendBuffer;
	std::string				_cgiContentType;
	bool					_isSent;
	LocationConfig			_location;
	bool					_isLocation;
	/* Fields for error page handling */
	std::string				_errorResolvedPath; // resolved filesystem path for custom error page
	std::string				_redirectLocation;  // external URL for redirects (http/https)
	bool					_hasErrorPageResolved; // true if an error page (or redirect) was resolved
public:
	char	**env;
	/*6. env público
	char** env es público y se setea desde fuera con setEnv. Cualquier parte del código puede modificarlo accidentalmente. Debería ser privado con solo el setter público.*/
	void	setEnv(char **envp);
	Client(Server& listener, int fd);
	~Client();
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
	bool		checkMethod(const std::string& method, const std::vector<std::string>& vec) const;
	void		checkPathValidity(std::string& path, std::vector<std::string>& index, bool autoindex, const std::string& root);
	bool		handleCgiIfNeeded();
	std::string	joinPath(const std::string& a, const std::string& b);
	bool		startCgiNonBlocking(const std::string& scriptPath, const std::string& interpreter);
	void		handleErrors();
	std::string	chargeDefaultErrorPage();
	void		chargeStatusData(const std::map<std::pair<int, int>, std::string>& errorPages);
	void		chargeStatusData(std::map<std::pair<int, int>, std::string>& errorPages);
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
	std::string getHeaderHost() const;
	void		setListener(const Server& s);
	bool		parentProcess(int *inpipe, int *outpipe, pid_t pid);

};

/*8. Demasiadas responsabilidades en Client
Client parsea HTTP, gestiona CGI, sirve ficheros, genera directory listing, maneja errores y envía respuestas. Para 42 es habitual, pero si hay bugs es difícil aislarlos. Mencionarlo para la evaluación.
Pasa el siguiente método cuando quieras.*/
#endif