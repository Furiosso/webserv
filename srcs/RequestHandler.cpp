#include "RequestHandler.hpp"
#include <dirent.h>
#include <iomanip>
#include <new>

RequestHandler::RequestHandler(Server& listener, int fd) : _listener(listener), _fd(fd), _error(0), _body(""), _isHeaderReady(false), _isBodyReady(false), _chunkLen(0), _chunkLine(""), _isSent(false)
{
	_headerContent.isChunked = false;
	_headerContent.isAutoindexResponse = false;
	_cgi.pid = -1;
	_cgi.in_fd = -1;
	_cgi.out_fd = -1;
	_cgi.write_pos = 0;
	_cgi.in_closed = false;
	_cgi.out_closed = false;
}

RequestHandler::~RequestHandler()
{
	if (_cgi.in_fd >= 0)
	{
		close(_cgi.in_fd);
		_cgi.in_fd = -1;
	}
	if (_cgi.out_fd >= 0)
	{
		close(_cgi.out_fd);
		_cgi.out_fd = -1;
	}
	if (_cgi.pid > 0)
	{
		int status = 0;
		pid_t w = waitpid(_cgi.pid, &status, WNOHANG);
		if (w == 0)
		{
			// child still running: try kill
			kill(_cgi.pid, SIGKILL);
			waitpid(_cgi.pid, &status, 0);
		}
		_cgi.pid = -1;
	}
}

Server	RequestHandler::getListener() const { return _listener; }

std::string RequestHandler::generateDirectoryListing(const std::string& dirPath, const std::string& requestPath)
{
	try {
		DIR* dir = opendir(dirPath.c_str());
		if (!dir)
			return std::string();
		std::string html;
		html += "<!doctype html><html><head><meta charset=\"utf-8\"><title>Index of ";
		html += requestPath + "</title></head><body>";
		html += "<h1>Index of " + requestPath + "</h1>\n<hr>\n<pre>";
		struct dirent* ent;
		std::vector<std::string> names;
		while ((ent = readdir(dir)) != NULL)
		{
			std::string n(ent->d_name);
			if (n == ".") continue;
			names.push_back(n);
		}
		closedir(dir);
		std::sort(names.begin(), names.end());
		for (size_t i = 0; i < names.size(); ++i)
		{
			std::string name = names[i];
			std::string href = requestPath;
			if (href.empty() || href[0] != '/')
				href = "/" + href;
			if (!href.empty() && href[href.size() - 1] != '/')
				href += "/";
			std::string fullPath = joinPath(dirPath, name);
			struct stat s;
			if (stat(fullPath.c_str(), &s) == 0)
				if (S_ISDIR(s.st_mode))
					name += "/";
			html += "<a href=\"" + href + name + "\">" + name + "</a>\n";
		}
		html += "</pre><hr></body></html>";
		return html;
	} catch (const std::bad_alloc& e) {
		std::cerr << "autoindex: std::bad_alloc while generating directory listing for " << dirPath << "\n";
		return std::string();
	}
}

void RequestHandler::setClientFd(int fd) { this->_fd = fd; }

int RequestHandler::getClientFd() const { return this->_fd; }

bool RequestHandler::getIsHeaderReady() const { return this->_isHeaderReady; }

bool RequestHandler::getIsBodyReady() const { return this->_isBodyReady; }

static std::string getMimeType(const std::string& path)
{
	if (path.size() >= 5 && path.substr(path.size() - 4) == ".jpg") return "image/jpeg";
	if (path.size() >= 6 && path.substr(path.size() - 5) == ".jpeg") return "image/jpeg";
	if (path.size() >= 5 && path.substr(path.size() - 4) == ".png") return "image/png";
	if (path.size() >= 5 && path.substr(path.size() - 4) == ".gif") return "image/gif";
	if (path.size() >= 5 && path.substr(path.size() - 4) == ".svg") return "image/svg+xml";
	if (path.size() >= 6 && path.substr(path.size() - 5) == ".html") return "text/html";
	if (path.size() >= 5 && path.substr(path.size() - 4) == ".htm") return "text/html";
	if (path.size() >= 5 && path.substr(path.size() - 4) == ".txt") return "text/plain";
	if (path.size() >= 6 && path.substr(path.size() - 5) == ".json") return "application/json";
	if (path.size() >= 5 && path.substr(path.size() - 4) == ".xml") return "application/xml";
	if (path.size() >= 5 && path.substr(path.size() - 4) == ".css") return "text/css";
	if (path.size() >= 4 && path.substr(path.size() - 3) == ".js") return "application/javascript";
	return "application/octet-stream";
}

// Decode percent-encoded URL path (e.g. %2B -> +, %23 -> #). Does not
// interpret '+' as space because in path components '+' is literal.
static int hexVal(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static std::string urlDecode(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == '%' && i + 2 < s.size())
		{
			int hi = hexVal(s[i + 1]);
			int lo = hexVal(s[i + 2]);
			if (hi >= 0 && lo >= 0)
			{
				char decoded = (char)((hi << 4) | lo);
				out.push_back(decoded);
				i += 2;
				continue;
			}
		}
		out.push_back(s[i]);
	}
	return out;
}

void RequestHandler::	chargeHeader()
{
	ssize_t bytesRead = recv(this->_fd, this->_buffer, sizeof(this->_buffer) - 1, 0); // sustituir el tamaño del buffer a una macro
	if (bytesRead < 0)
	{
		// In non-blocking mode, EAGAIN/EWOULDBLOCK mean "no data available now" — not a fatal error.
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		std::cerr << "Error reading from socket (fd " << this->_fd << "): " << strerror(errno) << std::endl;
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
		if (this->_header.find("\r\n\r\n") != std::string::npos) // End of headers
		{
			size_t headerEnd = this->_header.find("\r\n\r\n");
			this->_header = this->_header.substr(0, headerEnd);
            std::cout << "CHARGE HEADER1\n";
			this->parseHeader();
			std::cout << this->_header << std::endl;
            std::cout << "CHARGE HEADER2\n";
			try {
				this->setPath();
			} catch (const std::exception& e) {
				std::cerr << "Exception in setPath(): " << e.what() << std::endl;
				this->_error = 500;
			}
            std::cout << "CHARGE HEADER3\n";
			std::cout << "error: " << this->_error << std::endl;
			std::cout << "header path: " << this->_headerContent.path << std::endl;
			if(headerEnd + 4 < this->_header.size() && this->_headerContent.method == "POST")
				this->_body = this->_header.substr(headerEnd + 4);
			this->_request[0] = this->_header.substr(0, this->_header.find("\r\n")); // Request line
			_isHeaderReady = true;
			if (_headerContent.method != "POST")
				_isBodyReady = true;
		}
		ft_bzero(this->_buffer, sizeof(this->_buffer));
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
		dup2(inpipe[0], STDIN_FILENO);
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
	if (!isCgiRunning())
		return ;
	if (revents & (POLLHUP | POLLERR | POLLNVAL))
    {
        if (fd == _cgi.in_fd && _cgi.in_fd >= 0)
        {
            close(_cgi.in_fd);
            _cgi.in_fd = -1;
            _cgi.in_closed = true;
        }
        if (fd == _cgi.out_fd && _cgi.out_fd >= 0)
        {
            close(_cgi.out_fd);
            _cgi.out_fd = -1;
            _cgi.out_closed = true;
        }
        finalizeCgiIfDone();
        return;
    }
	// escribit a stdin del CGI cuando POLLOUT
	if (fd == _cgi.in_fd && (revents & POLLOUT))
	{
		while (_cgi.write_pos < _cgi.write_buf.size()) //si peta o algo cambiarlo por un if
		{
			const char* buf = _cgi.write_buf.c_str() + _cgi.write_pos;
			size_t to_write = _cgi.write_buf.size() - _cgi.write_pos;
			ssize_t	w = write(_cgi.in_fd, buf, to_write);
			if (w > 0)
				_cgi.write_pos += (size_t)w;
			else
			{
				close(_cgi.in_fd);
				_cgi.in_fd = -1;
				_cgi.in_closed = true;
				break ;
			}
		}
		if(_cgi.write_pos >= _cgi.write_buf.size() && _cgi.in_fd >= 0)
		{
			close(_cgi.in_fd);
			_cgi.in_fd = -1;
			_cgi.in_closed = true;
		}
	}
	// leer stdout del CGI cuando POLLIN
	if (fd == _cgi.out_fd && (revents & POLLIN))
	{
		char buf[4096];
		ssize_t r = read(_cgi.out_fd, buf, sizeof(buf));
		for (;;)
		{
			if (r > 0)
				_cgi.read_buf.append(buf, r);
			else if (r == 0)
			{
				 // EOF: child cerró stdout
				close(_cgi.out_fd);
				_cgi.out_fd = -1;
				_cgi.out_closed = true;
			}
			else
			{
				//error en read
				if (_cgi.out_fd >= 0)
					close(_cgi.out_fd);
				_cgi.out_fd = -1;
				_cgi.out_closed = true;
			}
			break ;
		}
	}
	finalizeCgiIfDone();
}

void RequestHandler::finalizeCgiIfDone()
{
	if (!isCgiRunning())
		return ;
	if (!_cgi.out_closed)
		return ;
	int	status = 0;
	pid_t w = waitpid(_cgi.pid, &status, WNOHANG);
	if (w == 0)
		return ;
	_cgi.pid = -1;
	std::string &out = _cgi.read_buf;
	size_t sep = out.find("\r\n\r\n");
	std::string cgiHeaders;
	std::string cgiBody;
	if (sep == std::string::npos)
		cgiBody = out;
	else
	{
		cgiHeaders = out.substr(0, sep);
		cgiBody = out.substr(sep + 4);
	}
	int cgiStatus = 200;
	if (!cgiHeaders.empty())
	{
		size_t pos = 0;
		while (pos < cgiHeaders.size())
		{
			size_t lineEnd = cgiHeaders.find("\r\n", pos);
            if (lineEnd == std::string::npos)
                lineEnd = cgiHeaders.size();
            std::string line = cgiHeaders.substr(pos, lineEnd - pos);
            // trim leading spaces
            size_t i = 0;
            while (i < line.size() && std::isspace(line[i]))
				++i;
            if (i < line.size())
                line = line.substr(i);
            // procesar "Status: 201 OK" o "Content-Length: N"
            if (line.size() >= 7 &&  line.find("Status:") == 0)
            {
                // extraer número de status en la línea
                size_t j = 7;
                while (j < line.size() && std::isspace(line[i]))
					++j;
                std::string num;
                while (j < line.size() && std::isdigit(line[i]))
				{
					num.push_back(line[j]);
					++j;
				}
                if (!num.empty())
                {
                    std::istringstream ss(num);
                    ss >> cgiStatus;
                }
            }
            // otras cabeceras (Content-Length/Content-Type) las puedes parsear si tienes estructura de respuesta
            pos = lineEnd + 2;
		}
	}
	this->_body = cgiBody;
	this->_error = cgiStatus;
	this->_isBodyReady = true;

	if (_cgi.in_fd >= 0)
	{
		close(_cgi.in_fd);
		_cgi.in_fd = -1;
	}
    if (_cgi.out_fd >= 0)
	{
		close(_cgi.out_fd);
		_cgi.out_fd = -1;
	}
    _cgi.in_closed = true;
    _cgi.out_closed = true;
}

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
	if (token != "GET" && token != "POST" && token != "DELETE" && token != "HEAD")
	{	
		std::cout << "que pasa\n";
		this->_error = 405;
		return ;
	}
	// Allow HEAD when GET is allowed on the server/location
	if (token == "HEAD")
	{
		std::string getToken = "GET";
		if (checkMethod(getToken, _listener.getConfig().allowed_methods) == false)
		{
			std::cout << "hola\n";
			_error = 405;
			return ;
		}
	}
	else if (checkMethod(token, _listener.getConfig().allowed_methods) == false)
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
	this->_headerContent.path = urlDecode(token);
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
		// Accept any header line that contains a ':' separating name and value.
		// Previous check relied on wordCounter which broke when header values contain ':' (e.g. URLs).
		size_t colonPos = line.find(":");
		if (colonPos == std::string::npos)
		{
			std::cout << "invalid header line: " << line << std::endl;
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
		if ((vegin + 1) != vend && (vegin + 2) != vend && strToLower(*vegin) == "content-length")
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
				this->_headerContent.ContentLength = num;
			}
		}
		if ((vegin + 1) != vend && (vegin + 2) != vend && strToLower(*vegin) == "transfer-encoding")
		{
			if (this->_headerContent.ContentLength == 0)
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
			std::cout << "it->path: " << it->path << " | _headerContent.path: " << _headerContent.path << " | path.size: " << it->path.size() << " | headercontent.path.size: " << _headerContent.path.size() << " | compare: " << _headerContent.path.compare(0, it->path.size(), it->path) << std::endl;
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
            	std::cout << "POST CHECK = _listener.getConfig().isAutoindex == true\n";
				if (it->isAutoindex == true)
					autoindex = it->autoindex;
            	std::cout << "POST AUTO == TRUE\n";
				if (it->isAlias)
				{
					std::string	rest = _headerContent.path.substr(it->path.size());
					_headerContent.path = it->root + rest;
					checkPathValidity(_headerContent.path, index, autoindex, it->root);
					return ;
				}
            	//std::cout << "POST CHECK\n";
				if (it->isRoot)
				{
					_headerContent.path = it->root + _headerContent.path;
					checkPathValidity(_headerContent.path, index, autoindex, it->root);
            		std::cout << "POST CHECK is root\n";
					return ;
				}
				_headerContent.path = _listener.getConfig().root + _headerContent.path;
				checkPathValidity(_headerContent.path, index, autoindex, _listener.getConfig().root);
				return ;
			}
		}
		if (_headerContent.path == "/")
		{
			if (!_listener.getConfig().index.empty())
					index = _listener.getConfig().index;
			if (_listener.getConfig().isAutoindex == true)
					autoindex = _listener.getConfig().autoindex;
			_headerContent.path = _listener.getConfig().root + _headerContent.path;
			checkPathValidity(_headerContent.path, index, autoindex, _listener.getConfig().root);
			return ;
		}
		std::cout << "perdona" << std::endl;
	}
	// If no location matched or there are no locations, default to server root
	
	std::vector<std::string> index;
	bool autoindex = false;
	if (!_listener.getConfig().index.empty())
		index = _listener.getConfig().index;
	if (_listener.getConfig().isAutoindex == true)
		autoindex = _listener.getConfig().autoindex;
	// prepend server root if the path is still absolute (starts with '/')
	if (!_headerContent.path.empty() && _headerContent.path[0] == '/')
	{
		_headerContent.path = _listener.getConfig().root + _headerContent.path;
		checkPathValidity(_headerContent.path, index, autoindex, _listener.getConfig().root);
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
	std::cout << "path:" <<path << " i: " << i << "AHHHHHHH\n";

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
		i = j;
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
		//i = j;
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

	if (c.empty() || ((c[0] != '/') && (c[0] == '.' && c[1] != '/')))
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
				//std::cout << "sale por aqui. Path: " << path << ". Root: " << root << std::endl;
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
							std::cout << "PATH EN IF: " << path << std::endl; 
							_error = 403;
							return ;
						}
						std::cout << "DENTRO DE IF\n";
						if (checkExtention(path, ".py") == true)
							;//comprobar si el path tiene una extension para cgi
						_headerContent.path = joinPath(_headerContent.path, *it);
						return ;
					}
				}
			}
			if (autoindex == true)
				// generar body de la respuesta con el listado del directorio
				// mark to produce autoindex HTML in sendResponse
				_headerContent.isAutoindexResponse = true;
			else
			{
				std::cout << "sale por aqui. Path: " << path << ". Root: " << root << std::endl;
				_error = 403;
			}
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

void	RequestHandler::chunkManagement() //corregir esto
{
	size_t		i;
	size_t		sublen;
	std::string	hexLen;

	this->_chunkLine += this->_buffer;
	while (true)
	{
		if (this->_chunkLen == 0)
		{
			i = this->_chunkLine.find("\r\n");
			if (i == std::string::npos)
				return ;
			hexLen = this->_chunkLine.substr(0, i);
			this->_chunkLen = hexToDecimal(hexLen);
			if (this->_chunkLen == 0)
			{
				if (this->_chunkLine[0] != '\r' || this->_chunkLine[1] != '\n')
    			{
        			this->_error = 400;
        			return ;
    			}
            	this->_chunkLine.erase(0, 2);
				this->_isBodyReady = true;
				return ;
			}
			this->_chunkLine.erase(0, i + 2);
		}
		sublen = this->_chunkLen;
		if (this->_chunkLine.size() < this->_chunkLen + 2)
		{
			sublen = this->_chunkLine.size();
			this->_chunkLen -= sublen;
			this->_body += this->_chunkLine.substr(0, sublen);
			this->_chunkLine.erase(0, sublen);
			return ;
		}
		if (this->_chunkLine[this->_chunkLen] != '\r'
			|| this->_chunkLine[this->_chunkLen + 1] != '\n')
		{
    		this->_error = 400;
    		return ;
		}
		this->_body += this->_chunkLine.substr(0, sublen);
    	this->_chunkLine.erase(0, this->_chunkLen + 2);
    	this->_chunkLen = 0;
	}
}

void	RequestHandler::chargeBody()
{
	if (this->_headerContent.method != "POST")
		return ;
	ssize_t bytesRead = recv(this->_fd, this->_buffer, sizeof(this->_buffer) - 1, 0); // sustituir el tamaño del buffer a una macro
	if (bytesRead < 0)
	{
		// Non-blocking sockets will often return EAGAIN/EWOULDBLOCK when there's no data yet.
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		std::cerr << "Error reading from socket (fd " << this->_fd << "): " << strerror(errno) << std::endl;
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
		if (this->_headerContent.isChunked == true)
		{
			chunkManagement();
			ft_bzero(this->_buffer, sizeof(this->_buffer));
			return ;
		}
		this->_body += this->_buffer; // Append to the request string
		if (this->_headerContent.ContentLength <= this->_body.size())
		{
			if (this->_body.size() > this->_headerContent.ContentLength)
				this->_body = this->_body.substr(0, this->_headerContent.ContentLength);
			this->_isBodyReady = true;
			this->_request[1] = this->_body;
		}
		ft_bzero(this->_buffer, sizeof(this->_buffer));
	}
}

void	RequestHandler::flushResponse()
{
	ssize_t		bytesSent;

	while (!_sendBuffer.empty())
	{
		bytesSent = send(this->_fd, _sendBuffer.c_str(), _sendBuffer.size(), 0);
		if (bytesSent > 0)
			_sendBuffer.erase(0, bytesSent);
		else if (bytesSent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			break ;
		else
			break ;
		
	}
	_isSent = _sendBuffer.empty();
}

void	RequestHandler::handleDelete()
{
	struct stat s;
	//if (!isWithinRoot())
	if (stat(_headerContent.path.c_str(), &s) == 0)
	{
		if (S_ISDIR(s.st_mode))
		{
			_error = 403;
			return ;
		}
	}
	if (std::remove(_headerContent.path.c_str()) !=  0)
	{
		if (errno == ENOENT)
        	_error = 404;
    	else if (errno == EACCES || errno == EPERM)
        	_error = 403;
    	else
        	_error = 500;
		return ;
	}
	_error = 204;
}

void	RequestHandler::sendResponse()
{	
	if (_sendBuffer.empty())
	{
		// Start with protocol and status
		_sendBuffer += this->_headerContent.protocol;
		if (this->_error >= 300 && this->_error < 600)
		{
			// TODO: generate proper error response bodies and headers
			//hacer un mensaje de error personalizado
			_sendBuffer += " 500 Internal Server Error\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
			_isSent = true;
			return ;
		}
		std::string body;
		// If path is a directory and autoindex response was requested, generate listing
		struct stat st;
		if (stat(this->_headerContent.path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && _headerContent.isAutoindexResponse)
		{
			// compute requestPath for links: try to remove server root prefix
			try
			{
				std::string requestPath = this->_headerContent.path;
				// naive: use basename relative to server root
				std::string root = _listener.getConfig().root;
				if (requestPath.find(root) == 0)
					requestPath = requestPath.substr(root.size());
				if (requestPath.empty()) requestPath = "/";
				body = generateDirectoryListing(this->_headerContent.path, requestPath);
			}
			catch (const std::bad_alloc& e)
			{
				std::cerr << "sendResponse: std::bad_alloc while preparing autoindex for " << this->_headerContent.path << "\n";
				this->_error = 500;
				body.clear();
			}
		}
		else
		{
			body = loadContent(this->_headerContent.path);
		}
		if (_headerContent.method == "GET")
		if (_headerContent.method == "POST")
		if (_headerContent.method == "DELETE")
			handleDelete();
		std::ostringstream hs;
		hs << " 200 OK\r\n";
		hs << "Content-Length: " << body.size() << "\r\n";
		// === INSERT SWITCH BY HTTP METHOD HERE ===
		// Single-line notes for implementation:
		// 1) Add: switch (this->_headerContent.method) { case "GET": case "HEAD": case "POST": case "DELETE": }
		// 2) For GET: if file -> body as loaded; if dir+autoindex -> body already generated above; set status 200.
		// 3) For HEAD: same as GET but do not append body later (respect method when adding body to _sendBuffer).
		// 4) For POST: if CGI -> initiate startCgiNonBlocking(...) here (only after body received) and set up _cgi.write_buf; set appropriate status (201/200) or delegate to CGI handling.
		// 5) For DELETE: try unlink(joinPath(root, requested)); set _error = 204 on success or 404/403 on failure.
		// 6) Ensure any filesystem ops check isWithinRoot(...) and access(..., R_OK/W_OK) before acting.
		// 7) After switch, prepare `body` (or leave empty for HEAD/204) and set content-type accordingly.
		// 8) Do NOT block for long operations; prefer non-blocking or spawn CGI as done elsewhere.
		// If we produced an autoindex listing, declare it as HTML so browsers render it instead of downloading
		if (this->_headerContent.isAutoindexResponse)
			hs << "Content-Type: text/html\r\n";
		else
			hs << "Content-Type: " << getMimeType(this->_headerContent.path) << "\r\n";
		hs << "Connection: close\r\n";
		hs << "\r\n";
		_sendBuffer += hs.str();
		if (this->_headerContent.method != "HEAD")
			_sendBuffer += body;
	}
	// Do not print binary response to stdout (can be very large and block logs)
	flushResponse();
}

/*std::string	RequestHandler::loadContent(const std::string& filename) const
{
	std::ifstream	file(filename.c_str(), std::ios::binary);

	file.seekg(0, std::ios::end);
	std::streamsize	size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::string	buffer(size, '\0');
	if (size > 0)
		file.read(&buffer[0], size);
	return buffer;
}*/

std::string	RequestHandler::loadContent(const std::string& filename) const
{
	std::ifstream	file(filename.c_str(), std::ios::binary);

	if (!file.is_open())
	{
		return std::string();
	}

	file.seekg(0, std::ios::end);
	std::streamoff soff = file.tellg();
	if (soff <= 0)
	{
		return std::string();
	}

	std::streamsize size = static_cast<std::streamsize>(soff);
	file.seekg(0, std::ios::beg);
	std::string buffer;
	buffer.resize(size);
	file.read(&buffer[0], size);
	return buffer;
}



bool	RequestHandler::getIsSent() const { return _isSent; }

