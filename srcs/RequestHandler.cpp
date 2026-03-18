#include "RequestHandler.hpp"

RequestHandler::RequestHandler(Server& listener, int fd) : _listener(listener), _fd(fd), _error(0), _body(""), _isHeaderReady(false), _isBodyReady(false)
{
	_headerContent.isChunked = false;
}

RequestHandler::~RequestHandler()
{
	_cgi.pid = -1;
	_cgi.in_fd = -1;
	_cgi.out_fd = -1;
	_cgi.write_pos = 0;
	_cgi.in_closed = false;
	_cgi.out_closed = false;
}

void RequestHandler::setClientFd(int fd) { this->_fd = fd; }

int RequestHandler::getClientFd() const { return this->_fd; }

bool RequestHandler::getIsHeaderReady() const { return this->_isHeaderReady; }

bool RequestHandler::getIsBodyReady() const { return this->_isBodyReady; }

void RequestHandler::chargeHeader()
{
	ssize_t bytesRead = recv(this->_fd, this->_buffer, sizeof(this->_buffer) - 1, 0); // sustituir el tamaño del buffer a una macro
	if (bytesRead < 0)
	{
		std::cerr << "bytesRead: " << bytesRead <<" fd: " << this->_fd << " _buffer: " << this->_buffer << " sizeof buffer: " << sizeof(this->_buffer) << std::endl;
		std::cerr << "Error reading from socket: " << strerror(errno) << std::endl;
		// Simplemente salir y esperar próxima notificación para este fd.
        return;
	}
	else if (bytesRead == 0)
	{
		// cliente cerró la conexión: marcar error para que el main limpie
        this->_error = 0; // o usa un flag específico; main debe detectar bytes==0 y cerrar
        // marca header/body como no listos y deja que main elimine este RequestHandler
        return;
	}
	else
	{
		this->_buffer[bytesRead] = '\0'; // Null-terminate the buffer
		this->_header += this->_buffer; // Append to the request string
		ft_bzero(this->_buffer, sizeof(this->_buffer));
		if (this->_header.find("\r\n\r\n") != std::string::npos) // End of headers
		{
			size_t headerEnd = this->_header.find("\r\n\r\n");
			if (headerEnd + 4 < this->_header.size())
				this->_body = this->_header.substr(headerEnd + 4);
			this->_header = this->_header.substr(0, headerEnd);
			this->parseHeader();
			std::cout << this->_header << std::endl;
			this->setPath();
			std::cout << "error: " << this->_error << std::endl;
			std::cout << "header path: " << this->_headerContent.path << std::endl;
			this->_request[0] = this->_header.substr(0, this->_header.find("\r\n")); // Request line
			_isHeaderReady = true;
		}
	}
}

bool	RequestHandler::checkMethod(std::string& method, const std::vector<std::string>& vec)
{
	if (std::find(vec.begin(), vec.end(), method) == vec.end())
		return false;
	return true; 
}

static bool setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) 
		return false;
    flags |= O_NONBLOCK;
    return (fcntl(fd, F_SETFL, flags) != -1);
}

bool RequestHandler::startCgiNonBlocking(const std::string& scriptPath, const std::string& interpreter)
{
	int	inpipe[2];
	int	outpipe[2];
	if (pipe(inpipe) == -1)
		return false;
	if (pipe(outpipe) == -1)
	{
		close(inpipe[0]);
		close(inpipe[1]);
		return false;
	}
	pid_t pid = fork();
	if (pid < 0)
	{
		close(inpipe[0]);
		close(inpipe[1]);
		close(outpipe[0]);
		close(outpipe[1]);
		return false;
	}
	else if (pid == 0)
	{
		dup2(inpipe[0], STDERR_FILENO);
		dup2(outpipe[1], STDOUT_FILENO);
		close(inpipe[0]);
		close(inpipe[1]);
		close(outpipe[0]);
		close(outpipe[1]);
		char *argv[3];
		argv[0] = const_cast<char*>(interpreter.c_str());
		argv[1] = const_cast<char*>(scriptPath.c_str());
		argv[2] = NULL;
		execve(argv[0], argv, NULL);
		_exit(127);
	}
	else
	{
		close(inpipe[0]);
		close(outpipe[1]);
		_cgi.pid = pid;
		_cgi.in_fd = inpipe[1];
		_cgi.out_fd = outpipe[0];
		_cgi.write_buf = this->_body;
		_cgi.write_pos = 0;
		_cgi.read_buf.clear();
		_cgi.in_closed = false;
		_cgi.out_closed = false;
		setNonBlocking(_cgi.in_fd);
		setNonBlocking(_cgi.out_fd);
		return true;
	}
	return false;
}

int RequestHandler::getCgiInFd() const
{
	return _cgi.in_fd;
}

int RequestHandler::getCgiOutFd() const
{
	return _cgi.out_fd;
}

bool RequestHandler::isCgiRunning() const
{
	return (_cgi.pid > 0);
}

void RequestHandler::handleCgiFdEvent(int fd, short revents)
{
	(void)fd;
	(void)revents;
}

void RequestHandler::finalizeCgiIfDone()
{}

void	RequestHandler::handleCgiIfNeeded()
{
	if (checkExtention(this->_headerContent.path, ".py"))
	{
		std::string interp = "/usr/bin/python3";
		if (!startCgiNonBlocking(this->_headerContent.path, interp))
			_error = 500;
	}
}

void RequestHandler::parseHeader()
{
	std::vector<std::string>	tokens;
	std::string					token;
	std::string					line;
	size_t						headerEnd;

	if (this->_error != 0)
		return ;
	headerEnd = this->_header.find("\r\n"); //Host: localhost\r\n
	line = this->_header.substr(0, headerEnd); //Host: localhost
	std::cout << "line: " << line << std::endl;
	this->_header = this->_header.substr(headerEnd + 2, this->_header.size()); //this->_header = this->_header.substr(headerEnd + 2);
	if (wordCounter(line, ' ') != 3)
	{
		std::cout << "me cago en sus muertos sera aqui" << std::endl;
		this->_error = 400;
		return ;
	}
	std::string::iterator	begin = line.begin();
	std::string::iterator	end = line.end();
	while (*begin == ' ')
		++begin;
	for (; begin != end; ++begin)
	{
		if (*begin == ' ')
			break;
		token.push_back(*begin);
	}
	if (token != "GET" && token != "POST" && token != "DELETE")
	{	
		std::cout << "que pasa\n";
		this->_error = 405;
		return ;
	}
	if (checkMethod(token, _listener.getConfig().allowed_methods) == false)
	{
		std::cout << "hola\n";
		_error = 405;
		return ;
	}
	this->_headerContent.method = token;
	std::cout << "method: " << token << std::endl;
	token = "";
	while (*begin == ' ')
		++begin;
	for (; begin != end; ++begin)
	{
		if (*begin == ' ')
			break;
		token.push_back(*begin);
	}
	if (token[0] != '/')
	{
		std::cout << "seguro que es aqui" << std::endl;
		_error = 400;
		return ;
	}
	this->_headerContent.path = token;
	handleCgiIfNeeded();
	/*if (checkExtention(this->_headerContent.path, ".py") == true)
		;//desviar el flujo hacia el gestor de cgi*/
	std::cout << "path: " << token << std::endl;
	token = "";
	while (*begin == ' ')
		++begin;
	for (; begin != end; ++begin)
		token.push_back(*begin);
	if (token != "HTTP/1.0" && token != "HTTP/1.1")
	{
		this->_error = 505;
		return ;
	}
	std::cout << "protocol: " << token << std::endl;
	this->_headerContent.protocol = token;
	while (*begin == ' ')
		++begin;
	begin += 2;
	while (this->_header.find("\r\n") != std::string::npos) // revisar ete bucle
	{
		line = this->_header.substr(0, this->_header.find("\r\n")); 
		if (wordCounter(line, ':') != 2 && line.substr(0, 4) != "Host")
		{
			std::cout << "line: " << line << std::endl;
			this->_error = 400;
			return ;
		}
		begin = line.begin();
		end = line.end();
		while (begin != end)
		{
			switch (*begin)
			{
				case ' ':
					if (!token.empty())
					{
						tokens.push_back(token);
						token = "";
					}
					break ;
				case ':':
					if (!token.empty())
					{
						tokens.push_back(token);
						token = "";
					}
					tokens.push_back(":");
					break ;
				default:
					token.push_back(*begin);
					//token = "";
			}
			++begin;
		}
		this->_header = this->_header.substr(this->_header.find("\r\n") + 2);
		std::cout << "this->_header:\n" << this->_header  << std::endl << std::endl;
	}
	std::vector<std::string>::iterator	vegin = tokens.begin();
	std::vector<std::string>::iterator	vend = tokens.end();
	std::stringstream					ss;
	size_t								num;
	for (; vegin != vend; ++vegin) // revisar este bucle
	{
		if ((vegin + 1) != vend && (vegin + 2) != vend && strToLower(*vegin) == "content-lenght")
		{
			if (this->_headerContent.isChunked == true)
			{
				this->_error = 404;
				return ;
			}
			if (*(vegin + 1) == ":")
			{
				ss << *(vegin + 2);
				ss >> num;
				checkContentLength(num);
				this->_headerContent.contentLenght = num;
			}
		}
		if ((vegin + 1) != vend && (vegin + 2) != vend && strToLower(*vegin) == "transfer-encoding")
		{
			if (this->_headerContent.contentLenght == 0)
			{
				this->_error = 404;
				return ;
			}
			if (*(vegin + 1) == ":" && *(vegin + 2) == "chunked")
				this->_headerContent.isChunked = true;
		}
	}
}

void	RequestHandler::checkContentLength(size_t num)
{
	const std::vector<LocationConfig>& locations = _listener.getConfig().locations;

	if (!locations.empty())
	{
		std::vector<LocationConfig>::const_iterator it = locations.begin();
		std::vector<LocationConfig>::const_iterator end = locations.end();
		
		for (; it != end; ++it)
		{
			if (it->path.size() <= _headerContent.path.size()
				&& _headerContent.path.compare(0, it->path.size(), it->path) == 0
				&& it->client_max_body_size != 0
				&& num > it->client_max_body_size)
			{
				this->_error = 413;
				return ;
			}
		}
	}
	if (this->_listener.getConfig().client_max_body_size != 0
		&& num > this->_listener.getConfig().client_max_body_size)
		this->_error = 413;
}

void	RequestHandler::setPath()
{
	const std::vector<LocationConfig>& locations = _listener.getConfig().locations;

	if (!locations.empty())
	{
		std::vector<LocationConfig>::const_iterator it = locations.begin();
		std::vector<LocationConfig>::const_iterator end = locations.end();
		std::vector<std::string>					index;
		bool										autoindex;

		autoindex = false;
		for (; it != end; ++it)
		{
			//std::cout << "it->path: " << it->path << " | _headerContent.path: " << _headerContent.path << " | path.size: " << it->path.size() << " | headercontent.path.size: " << _headerContent.path.size() << " | compare: " << _headerContent.path.compare(0, it->path.size(), it->path) << std::endl;
			if (it->path.size() <= _headerContent.path.size() && _headerContent.path.compare(0, it->path.size(), it->path) == 0)
			{
				std::cout << "it->path: " << it->path << std::endl;
				if (checkMethod(_headerContent.method, it->allowed_methods) == false)
				{
					std::cout << "method: |" << _headerContent.method << " size methods: " << it->allowed_methods.size() << "|\n";
					for (size_t i = 0; i < it->allowed_methods.size(); ++i)
					{
						std::cout << it->allowed_methods[i] << std::endl;
					}
					_error = 405;
					return ;
				}
				if (!_listener.getConfig().index.empty())
					index = _listener.getConfig().index;
				if (!it->index.empty())
					index = it->index;
				if (_listener.getConfig().isAutoindex == true)
					autoindex = _listener.getConfig().autoindex;
				if (it->isAutoindex == true)
					autoindex = it->autoindex;
				if (it->isAlias)
				{
					std::string	rest = _headerContent.path.substr(it->path.size());
					_headerContent.path = it->root + rest;
					checkPathValidity(_headerContent.path, index, autoindex, it->root);
					return ;
				}
				if (it->isRoot)
				{
					_headerContent.path = it->root + _headerContent.path;
					checkPathValidity(_headerContent.path, index, autoindex, it->root);
					return ;
				}
				_headerContent.path = _listener.getConfig().root + _headerContent.path;
				checkPathValidity(_headerContent.path, index, autoindex, _listener.getConfig().root);
				return ;
			}
		}
		std::cout << "perdona" << std::endl;
	}
}

static std::string	normalizePath(const std::string& p)
{
	std::string path = p;
	if (path.empty())
		return std::string("/");
	bool absolute = (path[0] =='/');
	std::vector<std::string> parts;
	size_t	i = 0;
	while (i < path.size())
	{
		// skip consecutive '/'
		while (i < path.size() && path[i] == '/')
			++i;
		if (i >= path.size())
			break;
		size_t j = i;
		while (j < path.size() && path[j] != '/')
			++j;
		std::string	token = path.substr(i, j - i);
		if (token == "." || token.empty())
			continue;
		else if (token == "..")
		{
			if (!parts.empty() && parts.back() != "..")
				parts.pop_back();
			else if (!absolute)
				parts.push_back("..");
		}
		else
			parts.push_back(token);
		i = j;
	}
	std::string out;
	if (absolute)
		out = "/";
	for (size_t k = 0; k < parts.size(); ++k)
	{
		if (!(absolute && k == 0) && out.size() > 0 && out[out.size() -1] != '/')
			out += "/";
		out += parts[k];
	}
	if (out.empty())
	{
		if (absolute)
			out = std::string("/");
		else
			out = std::string(".");
	}
	if (out.size() > 1 && out[out.size() - 1] == '/')
		out.erase(out.size() - 1);
	return out;
}

static bool	isWithinRoot(const std::string& candidate, const std::string& root)
{
	std::string	r = normalizePath(root);
	std::string	c = normalizePath(candidate);

	if (c.empty() || c[0] != '/')
		return false;
	if (r == "/")
		return true;
	if (r.size() > 1 && r[r.size() - 1] == '/')	
		r.erase(r.size() - 1);
	if (c == r)
		return true;
	if (c.size() > r.size() && c.compare(0, r.size(), r) == 0 && c[r.size()] == '/')
		return true;
	return false;
}

void	RequestHandler::checkPathValidity(std::string& path, std::vector<std::string>& index, bool autoindex, const std::string& root)
{
	struct stat st;

	if (stat(path.c_str(), &st) == 0)
	{
    	if (S_ISREG(st.st_mode))
		{
			if (access(path.c_str(), R_OK) != 0)
			{
				_error = 404;
				return ;
			}
			if (!isWithinRoot(path, root))
			{
				_error = 403;
				return ;
			}
			return ;
		}
    	if (S_ISDIR(st.st_mode))
        {
			if (!index.empty())
			{
				std::vector<std::string>::iterator	it = index.begin();
				std::vector<std::string>::iterator	end = index.end();
				std::string							needle;
				for (; it != end; ++it)
				{
					std::cout << "index: " << joinPath(path, *it) << std::endl;
					if (access(joinPath(path, *it).c_str(), F_OK) == 0)
					{
						if (access(joinPath(path, *it).c_str(), R_OK) != 0)
						{
							_error = 404;
							return ;
						}
						if (!isWithinRoot(joinPath(path, *it), root))
						{
							_error = 403;
							return ;
						}
						if (checkExtention(path, ".py") == true)
							;//comprobar si el path tiene una extension para cgi
						_headerContent.path = joinPath(_headerContent.path, *it);
						return ;
					}
				}
			}
			if (autoindex == true)
				;//generar body de la respuesta con el listado del directorio
			else
				_error = 403;
			return ;
		}
	}
	_error = 404;
}

std::string	RequestHandler::joinPath(const std::string& a, const std::string& b) // revisar esta funcion
{
    if (a[a.size() - 1] == '/')
	{
    	return a + b;
	}
	return a + "/" + b;
}

void	RequestHandler::chargeBody()
{
	if (this->_headerContent.isChunked == true)
	{
		//chunkManagement();
		return ;
	}
	ssize_t bytesRead = recv(this->_fd, this->_buffer, sizeof(this->_buffer) - 1, 0); // sustituir el tamaño del buffer a una macro
	if (bytesRead < 0)
	{
		std::cerr << "bytesRead: " << bytesRead <<" fd: " << this->_fd << " _buffer: " << this->_buffer << " sizeof buffer: " << sizeof(this->_buffer) << std::endl;
		std::cerr << "Error reading from socket: " << strerror(errno) << std::endl;
		// Simplemente salir y esperar próxima notificación para este fd.
        return;
	}
	else if (bytesRead == 0)
	{
		// cliente cerró la conexión: marcar error para que el main limpie
        this->_error = 0; // o usa un flag específico; main debe detectar bytes==0 y cerrar
        // marca header/body como no listos y deja que main elimine este RequestHandler
        return;
	}
	else
	{
		this->_buffer[bytesRead] = '\0'; // Null-terminate the buffer
		this->_body += this->_buffer; // Append to the request string
		if (this->_headerContent.contentLenght <= this->_body.size())
		{
			if (this->_body.size > this->_headerContent.contentLenght)
				this->_body = this->_body.substr(0, this->_headerContent.contentLenght);
			this->_isBodyReady = true;
		}
		ft_bzero(this->_buffer, sizeof(this->_buffer));
	}
}