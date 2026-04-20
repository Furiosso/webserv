#include "Client.hpp"
#include <dirent.h>
#include <iomanip>
#include <limits.h>
#include <stdlib.h>
#include <new>
#include <cstring>
#include <strings.h>
#include <sstream>

Client::Client(Server& listener, int fd) : _listener(listener), _fd(fd), _status(200), _body(""), _isHeaderReady(false), _isBodyReady(false), _chunkLen(0), _chunkLine(""), _isSent(false), _isLocation(false)
{
	_headerContent.isChunked = false;
	_headerContent.ContentLength = 0;
	_headerContent.isAutoindexResponse = false;
	_hasErrorPageResolved = false;
	_cgi.pid = -1;
	_cgi.in_fd = -1;
	_cgi.out_fd = -1;
	_cgi.write_pos = 0;
	_cgi.in_closed = false;
	_cgi.out_closed = false;
	_cgi.finalized = false;

	// do not duplicate env; caller may set it via setEnv
	env = NULL;
}

Client::~Client()
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

void Client::setEnv(char **envp)
{
	// store pointer only; do not allocate or free memory
	env = envp;
}

void Client::setListener(const Server& s) { this->_listener = s; }

std::string Client::getHeaderHost() const { return this->_headerContent.host; }

Server	Client::getListener() const { return _listener; }

std::string Client::generateDirectoryListing(const std::string& dirPath, const std::string& requestPath)
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

void Client::setClientFd(int fd) { this->_fd = fd; }

int Client::getClientFd() const { return this->_fd; }

bool Client::getIsHeaderReady() const { return this->_isHeaderReady; }

bool Client::getIsBodyReady() const { return this->_isBodyReady; }

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

static const char* reasonPhrase(int code)
{
	switch (code)
	{
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 415: return "Unsupported Media Type";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";
		default: return "Error";
	}
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

// Return directory part of a filesystem path, or empty if none.
static std::string getDirectory(const std::string& path)
{
	if (path.empty()) return std::string();
	// Remove leading "./" segments which indicate current directory
	std::string p = path;
	while (p.size() >= 2 && p[0] == '.' && p[1] == '/')
		p = p.substr(2);
	size_t pos = p.find_last_of('/');
	if (pos == std::string::npos) return std::string();
	if (pos == 0) return std::string("/");
	return p.substr(0, pos);
}

void Client::	chargeHeader()
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
		// cliente cerró la conexión: puede significar EOF after body
		if (this->_headerContent.method == "POST")
		{
			// If Content-Length was set and we've already read enough, mark body ready
			if (!this->_headerContent.isChunked)
			{
				if (this->_headerContent.ContentLength != 0 && this->_body.size() >= this->_headerContent.ContentLength)
				{
					if (this->_body.size() > this->_headerContent.ContentLength)
						this->_body = this->_body.substr(0, this->_headerContent.ContentLength);
					this->_isBodyReady = true;
					this->_request[1] = this->_body;
					std::cout << "chargeBody: EOF but body complete, size=" << this->_body.size() << "\n";
					return;
				}
				// If no Content-Length (unlikely) but some body present, consider it ready
				if (this->_headerContent.ContentLength == 0 && !this->_body.empty())
				{
					this->_isBodyReady = true;
					this->_request[1] = this->_body;
					std::cout << "chargeBody: EOF with body and no Content-Length, size=" << this->_body.size() << "\n";
					return;
				}
			}
			// Otherwise, treat as client closed prematurely -> mark error so main will cleanup
			this->_status = 400;
			return;
		}
		// For non-POST methods: mark as closed so main will cleanup
		this->_status = 200;
		return;
	}
	else
	{
		this->_buffer[bytesRead] = '\0'; // Null-terminate the buffer
		this->_header += this->_buffer; // Append to the request string
		if (this->_header.find("\r\n\r\n") != std::string::npos) // End of headers
		{
			size_t headerEnd = this->_header.find("\r\n\r\n");
			// Preserve the full buffer so we can extract a body that may have
			// been received together with the headers in the same recv().
			std::string full = this->_header;
			this->_header = full.substr(0, headerEnd);
			std::cout << "chargeHeader: headerEnd=" << headerEnd << " full.size=" << full.size() << "\n";
            std::cout << "CHARGE HEADER1\n";
			this->parseHeader();
			// If client included 'Expect: 100-continue' we must acknowledge it
			// so browsers will proceed to send the request body.
			
			std::string lower = full;
			lower = strToLower(lower);
			if (lower.find("expect: 100-continue") != std::string::npos)
			{
				const char *cont = "HTTP/1.1 100 Continue\r\n\r\n";
				ssize_t s = send(this->_fd, cont, std::strlen(cont), 0);
				(void)s;
			}
			
			std::cout << "after parseHeader: ContentLength=" << this->_headerContent.ContentLength << " isChunked=" << this->_headerContent.isChunked << " method=" << this->_headerContent.method << "\n";
			std::cout << this->_header << std::endl;
            std::cout << "CHARGE HEADER2\n";
			try {
				this->setPath();
			} catch (const std::exception& e) {
				std::cerr << "Exception in setPath(): " << e.what() << std::endl;
				this->_status = 500;
			}
            std::cout << "CHARGE HEADER3\n";
			std::cout << "error: " << this->_status << std::endl;
			std::cout << "header path: " << this->_headerContent.path << std::endl;
			if (this->_headerContent.method == "POST")
			{
				if (headerEnd + 4 < full.size())
				{
					std::string extra = full.substr(headerEnd + 4);
					// If Transfer-Encoding: chunked, feed chunk data into chunkLine and try to parse
					if (this->_headerContent.isChunked)
					{
						this->_chunkLine += extra;
						chunkManagement();
					}
					else
					{
						this->_body = extra;
						// If we already have the full body according to Content-Length, mark ready
						if (this->_headerContent.ContentLength != 0 && this->_body.size() >= this->_headerContent.ContentLength)
						{
							if (this->_body.size() > this->_headerContent.ContentLength)
								this->_body = this->_body.substr(0, this->_headerContent.ContentLength);
							this->_isBodyReady = true;
							this->_request[1] = this->_body;
							std::cout << "CHARGE HEADER: body already complete, size=" << this->_body.size() << "\n";
						}
						else if (this->_body.size() > 0 && this->_headerContent.ContentLength == 0)
						{
							// Fallback: headers parsing didn't capture Content-Length but body bytes arrived
							/*This branch triggers when some bytes have been accumulated in this->_body but the parsed headers report ContentLength == 0.
							In that case the code treats the currently buffered bytes as the full request body: it sets headerContent.
							ContentLength to the buffer size, marks the body ready with _isBodyReady = true, copies the buffer into _request[1], and emits a debug message with the size.
							
							The intent is a simple fallback for situations where header parsing failed to capture a Content-Length value (or the header was omitted) but body bytes nonetheless arrived.
							By stamping the header with the observed length and marking the body ready, the rest of the request pipeline can proceed as if a Content-Length had been provided.
							
							Suggested improvements:
							- verify the request method and Transfer-Encoding before applying this fallback
								(only treat the buffer-as-body when HTTP framing semantics allow it, e.g., when the connection will be closed to signal end-of-body).
							- Add size limits and validation, ensure request has the expected structure before writing to index 1, replace std::cout with a proper logger,
								and add explicit handling for chunked encoding or for reading until connection close if Content-Length is absent.
							These changes make the fallback safer and more robust.
							*/
							this->_headerContent.ContentLength = this->_body.size();
							this->_isBodyReady = true;
							this->_request[1] = this->_body;
							std::cout << "CHARGE HEADER: fallback mark body ready, size=" << this->_body.size() << "\n";
						}
					}
				}
			}
			this->_request[0] = this->_header.substr(0, this->_header.find("\r\n")); // Request line
			_isHeaderReady = true;
			if (_headerContent.method != "POST")
				_isBodyReady = true;
		}
		ft_bzero(this->_buffer, sizeof(this->_buffer));
	}
}

bool	Client::checkMethod(std::string& method, const std::vector<std::string>& vec)
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

/* Build a new envp (char*[] terminated by NULL) by merging parent_env and
   extra variables (extras). The returned envp is allocated with new[] and
   each string with new[]. Caller must free with freeEnvp(). */
/*static char **buildEnvpFromMapAndParent(const std::map<std::string, std::string>& extras, char **parent_env)
{
	std::map<std::string, std::string> merged;
	if (parent_env)
	{
		for (char **p = parent_env; *p != NULL; ++p)
		{
			std::string s(*p);
			size_t eq = s.find('=');
			if (eq == std::string::npos) continue;
			std::string k = s.substr(0, eq);
			std::string v = s.substr(eq + 1);
			merged[k] = v;
		}
	}
	for (std::map<std::string, std::string>::const_iterator it = extras.begin(); it != extras.end(); ++it)
		merged[it->first] = it->second; // override or insert

	char **envp = new char*[merged.size() + 1];
	size_t i = 0;
	for (std::map<std::string,std::string>::const_iterator it = merged.begin(); it != merged.end(); ++it)
	{
		std::string kv = it->first + "=" + it->second;
		envp[i] = new char[kv.size() + 1];
		std::strncpy(envp[i], kv.c_str(), kv.size() + 1);
		++i;
	}
	envp[i] = NULL;
	return envp;
}

static void freeEnvp(char **envp)
{
	if (!envp) return;
	for (char **p = envp; *p != NULL; ++p)
		delete [] *p;
	delete [] envp;
}*/

bool Client::startCgiNonBlocking(const std::string& scriptPath, const std::string& interpreter)
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
	/*while (*env)
		std::cout <<"pid: " << pid<< "ENV:::::::::::::::::" <<*(env)++ << std::endl;*/
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
		/* child: set up stdio from pipes */
		if (dup2(inpipe[0], STDIN_FILENO) == -1) _exit(127);
		if (dup2(outpipe[1], STDOUT_FILENO) == -1) _exit(127);
		/* close both ends of pipes in child */
		close(inpipe[0]);
		close(inpipe[1]);
		close(outpipe[0]);
		close(outpipe[1]);
		std::string dir = getDirectory(scriptPath);
		bool changedCwd = false;
		if (!dir.empty())
		{
			// try to resolve to an absolute path first
			char resolved[PATH_MAX];
			if (realpath(dir.c_str(), resolved) != NULL)
			{
				if (chdir(resolved) != 0)
				{
					int se = errno;
					std::cerr << "child: chdir to '" << resolved << "' failed: " << strerror(se) << "\n";
					_exit(127);
				}
			}
			else
			{
				if (chdir(dir.c_str()) != 0)
				{
					int se = errno;
					std::cerr << "child: chdir to '" << dir << "' failed: " << strerror(se) << "\n";
					_exit(127);
				}
			}
			changedCwd = true;
		}

		/* Build argv dynamically. If interpreter looks like Python, request unbuffered mode. 
		std::vector<char*> argv_vec;
		bool python_unbuffered = false;
		if (!interpreter.empty())
		{
			argv_vec.push_back(const_cast<char*>(interpreter.c_str()));
			std::string li = interpreter;
			if (li.find("python") != std::string::npos)
			{
				// request unbuffered python to avoid stdout buffering delaying the response
				argv_vec.push_back(const_cast<char*>("-u"));
				python_unbuffered = true;
			}
			argv_vec.push_back(const_cast<char*>(scriptPath.c_str()));
		}
		else
		{
			argv_vec.push_back(const_cast<char*>(scriptPath.c_str()));
		}
		argv_vec.push_back(NULL);
		char **argv_exec = new char*[argv_vec.size()];
		for (size_t ai = 0; ai < argv_vec.size(); ++ai) argv_exec[ai] = argv_vec[ai];

		 //Build richer CGI env from available request data and inherit parent's env
		std::map<std::string,std::string> extras;
		extras["REQUEST_METHOD"] = _headerContent.method;
		extras["SERVER_PROTOCOL"] = _headerContent.protocol;
		if (_headerContent.ContentLength > 0)
		{
			std::ostringstream __tmp_ss;
			__tmp_ss << _headerContent.ContentLength;
			extras["CONTENT_LENGTH"] = __tmp_ss.str();
		}
		if (!this->_headerContent.host.empty())
			extras["SERVER_NAME"] = this->_headerContent.host;
		// SCRIPT_FILENAME = filesystem path to script
		extras["SCRIPT_FILENAME"] = scriptPath;
		// Try to obtain original request URI (path + optional ?query)
		std::string request_uri;
		if (!this->_request[0].empty())
		{
			// request line stored as: "GET /path?query HTTP/1.1"
			std::istringstream rs(this->_request[0]);
			std::string method_token, uri_token, proto_token;
			rs >> method_token >> uri_token >> proto_token;
			request_uri = uri_token;
		}
		if (request_uri.empty())
		{
			// fallback: try to reconstruct from headerContent.path (may be filesystem path)
			request_uri = _headerContent.path;
		}
		// split QUERY_STRING if present
		size_t qpos = request_uri.find('?');
		if (qpos != std::string::npos)
		{
			extras["SCRIPT_NAME"] = request_uri.substr(0, qpos);
			extras["QUERY_STRING"] = request_uri.substr(qpos + 1);
		}
		else
		{
			extras["SCRIPT_NAME"] = request_uri;
			extras["QUERY_STRING"] = std::string();
		}
		extras["REQUEST_URI"] = request_uri;
		// DOCUMENT_ROOT: prefer location root if available
		if (_isLocation)
			extras["DOCUMENT_ROOT"] = _location.root;
		else
			extras["DOCUMENT_ROOT"] = _listener.getConfig().root;
		// Standard CGI vars
		extras["GATEWAY_INTERFACE"] = "CGI/1.1";
		extras["SERVER_SOFTWARE"] = "webserv/0.1";

		// If python, set PYTHONUNBUFFERED in the child env too
		if (python_unbuffered)
			extras["PYTHONUNBUFFERED"] = "1";

		char **child_envp = buildEnvpFromMapAndParent(extras, env);

		std::cerr << "child: execve with argv[0]=" << argv_exec[0] << "\n";
		execve(argv_exec[0], argv_exec, child_envp);

		// execve failed 
		int savedErrno = errno;
		std::cerr << "child: execve failed: errno=" << savedErrno << " (" << strerror(savedErrno) << ")\n";
		freeEnvp(child_envp);
		delete [] argv_exec;*/
		// Build argv safely into owned strings so c_str() pointers remain valid
		std::vector<std::string> argv_store;
		if (!interpreter.empty())
		{
			argv_store.push_back(interpreter);
			std::string li = interpreter;
			if (li.find("python") != std::string::npos)
			{
				argv_store.push_back(std::string("-u"));
			}
			std::string childScript = scriptPath;
			while (childScript.size() >= 2 && childScript[0] == '.' && childScript[1] == '/')
				childScript = childScript.substr(2);
			if (changedCwd)
			{
				size_t bp = childScript.find_last_of('/');
				if (bp != std::string::npos)
					childScript = childScript.substr(bp + 1);
			}
			argv_store.push_back(childScript);
		}
		else
		{
			argv_store.push_back(scriptPath);
		}
		size_t argc = argv_store.size();
		char **argv_exec = new char*[argc + 1];
		for (size_t ai = 0; ai < argc; ++ai)
			argv_exec[ai] = const_cast<char*>(argv_store[ai].c_str());
		argv_exec[argc] = NULL;
		// Debug: print full argv list
		std::cerr << "child argv:";
		for (size_t ai = 0; ai < argc + 1; ++ai)
			std::cerr << " '" << (argv_exec[ai] ? argv_exec[ai] : (char*)"(null)") << "'";
		std::cerr << std::endl;
		// Attempt to execute interpreter; on failure, print errno reason to stderr
		execve(argv_exec[0], argv_exec, env);
		int savedErrno = errno;
		std::cerr << "execve failed: errno=" << savedErrno << " (" << strerror(savedErrno) << ")\n";
		/*std::cerr << "child: execve failed: errno=" << savedErrno << " (" << strerror(savedErrno) << ")\n";
		freeEnvp(child_envp);
		delete [] argv_exec;*/
		// Ensure child exits without flushing stdio buffers from parent state
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
		std::cout << "BODY EN CGI" <<_body << std::endl;
		_cgi.write_pos = 0;
		_cgi.read_buf.clear();
		// Clear any previous CGI content-type
		_cgiContentType.clear();
		_cgi.in_closed = false;
		_cgi.out_closed = false;
		_cgi.finalized = false;
		setNonBlocking(_cgi.in_fd);
		setNonBlocking(_cgi.out_fd);
		// If there's no data to send to CGI stdin (common for GET/HEAD), close
		// the write end in the parent to signal EOF to the child immediately.
		// This prevents scripts that read from stdin from blocking.
		if (_cgi.write_buf.empty() && _cgi.in_fd >= 0)
		{
			close(_cgi.in_fd);
			_cgi.in_fd = -1;
			_cgi.in_closed = true;
		}
		std::cerr << "startCgiNonBlocking (parent): pid=" << pid << " in_fd=" << _cgi.in_fd << " out_fd=" << _cgi.out_fd << " write_buf_size=" << _cgi.write_buf.size() << " in_closed=" << _cgi.in_closed << "\n";
		//_isSent = true;
		return true;
	}
	return false;
}

int Client::getCgiInFd() const
{
	return _cgi.in_fd;
}

std::string Client::getMethod() const
{
	return _headerContent.method;
}

int Client::getCgiOutFd() const
{
	return _cgi.out_fd;
}

bool Client::isCgiRunning() const
{
	return (_cgi.pid > 0);
}

void Client::handleCgiFdEvent(int fd, short revents)
{
	if (!isCgiRunning())
		return ;
	std::cerr << "handleCgiFdEvent: fd=" << fd << " revents=" << revents << "\n";
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
	// escribir a stdin del CGI cuando POLLOUT
	if (fd == _cgi.in_fd && (revents & POLLOUT))
	{
		while (_cgi.write_pos < _cgi.write_buf.size())
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
		std::cerr << "handleCgiFdEvent: fd=" << fd << " read=" << r << std::endl;
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
	}
	finalizeCgiIfDone();
}

void Client::finalizeCgiIfDone()
{
	if (!isCgiRunning())
	{
		// If already finalized previously (cgi finished and parsed), nothing to do
		if (_cgi.finalized)
			return;
		std::cerr << "cacota gorda\n";
		return ;
	}
	int	status = 0;
	std::cerr << "finalizeCgiIfDone: checking pid=" << _cgi.pid << " out_closed=" << _cgi.out_closed << " read_buf_size=" << _cgi.read_buf.size() << "\n";
	// Check whether the CGI child has exited. If it has, try to drain any remaining
	// data from the child's stdout fd before parsing. This allows finalization even
	// when the POLLHUP/EOF handling hasn't yet marked out_closed.
	pid_t w = waitpid(_cgi.pid, &status, WNOHANG);
	std::cerr << "finalizeCgiIfDone: waitpid returned w=" << w << " errno=" << errno << "\n";
	if (w == 0)
	{
		// If the child hasn't yet been reaped but we've already received a
		// complete CGI response in _cgi.read_buf (headers + body separator),
		// it's safe to block briefly and reap the child so we can finalize
		// the response immediately instead of waiting for another poll event.
		bool haveSep = (_cgi.read_buf.find("\r\n\r\n") != std::string::npos) || (_cgi.read_buf.find("\n\n") != std::string::npos);
		if (!haveSep)
			return ;
		std::cerr << "finalizeCgiIfDone: response looks complete, waiting for child to exit...\n";
		// Block until child exits to avoid leaving a zombie; this should be
		// fast for well-behaved CGI scripts that wrote a complete response.
		w = waitpid(_cgi.pid, &status, 0);
		std::cerr << "finalizeCgiIfDone: blocking waitpid returned w=" << w << " errno=" << errno << "\n";
	}
	// Child exited or changed state; mark pid as cleared and attempt to read any
	// remaining data from the out_fd so we have a complete response body.
	_cgi.pid = -1;
	if (_cgi.out_fd >= 0)
	{
		// Drain remaining data (non-blocking fd) until EOF or error
		char tmpbuf[4096];
		while (true)
		{
			ssize_t r = read(_cgi.out_fd, tmpbuf, sizeof(tmpbuf));
			if (r > 0)
				_cgi.read_buf.append(tmpbuf, r);
			else if (r == 0)
			{
				close(_cgi.out_fd);
				_cgi.out_fd = -1;
				_cgi.out_closed = true;
				break;
			}
			else
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK)
				{
					// No more data available right now; leave fd open and proceed
					break;
				}
				// On other errors, close the fd and mark closed
				close(_cgi.out_fd);
				_cgi.out_fd = -1;
				_cgi.out_closed = true;
				break;
			}
		}
	}
	std::string &out = _cgi.read_buf;
	// Accept both CRLFCRLF and LFLF as header/body separators from CGI output
	size_t sep = out.find("\r\n\r\n");
	std::string cgiHeaders;
	std::string cgiBody;
	if (sep != std::string::npos)
	{
		cgiHeaders = out.substr(0, sep);
		cgiBody = out.substr(sep + 4);
	}
	else
	{
		sep = out.find("\n\n");
		if (sep != std::string::npos)
		{
			cgiHeaders = out.substr(0, sep);
			cgiBody = out.substr(sep + 2);
		}
		else
		{
			// No header separator found: treat entire output as body
			cgiBody = out;
		}
	}
	int cgiStatus = 200;
	if (!cgiHeaders.empty())
	{
		// Accept either CRLF or LF line separators from CGI output
		size_t pos = 0;
		while (pos < cgiHeaders.size())
		{
			size_t lineEnd = cgiHeaders.find('\n', pos);
			if (lineEnd == std::string::npos)
				lineEnd = cgiHeaders.size();
			// trim possible \r at end
			size_t len = lineEnd - pos;
			if (len > 0 && cgiHeaders[lineEnd - 1] == '\r')
				--len;
			std::string line = cgiHeaders.substr(pos, len);
			// trim leading spaces
			size_t i = 0;
			while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
			if (i < line.size()) line = line.substr(i);

			if (line.size() >= 7 && line.find("Status:") == 0)
			{
				// extract numeric status
				size_t j = 7;
				while (j < line.size() && std::isspace(static_cast<unsigned char>(line[j]))) ++j;
				std::string num;
				while (j < line.size() && std::isdigit(static_cast<unsigned char>(line[j]))) { num.push_back(line[j]); ++j; }
				if (!num.empty()) { std::istringstream ss(num); ss >> cgiStatus; }
			}
			else if (line.size() >= 13 && strncasecmp(line.c_str(), "Content-Type:", 13) == 0)
			{
				size_t k = 13;
				while (k < line.size() && std::isspace(static_cast<unsigned char>(line[k]))) ++k;
				_cgiContentType = line.substr(k);
			}
			else if (line.size() >= 15 && strncasecmp(line.c_str(), "Content-Length:", 15) == 0)
			{
				size_t k = 15;
				while (k < line.size() && std::isspace(static_cast<unsigned char>(line[k]))) ++k;
				std::string num = line.substr(k);
				std::istringstream ss(num);
				size_t clen = 0; ss >> clen;
				// optional: validate length vs cgiBody.size()
			}
			pos = lineEnd + 1;
		}
	}
	this->_body = cgiBody;
	this->_status = cgiStatus;
	this->_isBodyReady = true;
	_cgi.finalized = true;

	std::cerr << "finalizeCgiIfDone: client fd=" << this->_fd << " status=" << this->_status << " body_size=" << this->_body.size() << "\n";

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
	//_isSent = true;

	// Try to send the response immediately now that CGI finished and _body/_status are set.
	// main will also trigger sendResponse() via poll when appropriate, but sending here
	// avoids waiting an extra poll iteration and unblocks clients faster.
	this->sendResponse();
}

bool	Client::handleCgiIfNeeded()
{
	// Build CGI map: server-level then overlay location-level
	std::map<std::string,std::string> cgiMap = _listener.getConfig().cgi;
	if (this->_isLocation)
	{
		std::map<std::string,std::string>::const_iterator it = _location.cgi.begin();
		for (; it != _location.cgi.end(); ++it)
			cgiMap[it->first] = it->second;
	}
	std::string ext = getExtension(this->_headerContent.path);
	if (cgiMap.count(ext))
	{
		std::string interp = cgiMap[ext];
		if (!startCgiNonBlocking(this->_headerContent.path, interp))
		{
			_status = 500;
			return false;
		}
		return true;
	}
	return false;
}

void Client::parseHeader()
{
	std::vector<std::string>	tokens;
	std::string					token;
	std::string					line;
	size_t						headerEnd;

	if (this->_status != 200)
		return ;
	headerEnd = this->_header.find("\r\n"); //Host: localhost\r\n
	line = this->_header.substr(0, headerEnd); //Host: localhost
	std::cout << "line: " << line << std::endl;
	this->_header = this->_header.substr(headerEnd + 2, this->_header.size()); //this->_header = this->_header.substr(headerEnd + 2);
	if (wordCounter(line, ' ') != 3)
	{
		std::cout << "me cago en sus muertos sera aqui" << std::endl;
		this->_status = 400;
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
		this->_status = 405;
		return ;
	}
	// Allow HEAD when GET is allowed on the server/location
	if (token == "HEAD")
	{
		std::string getToken = "GET";
		if (checkMethod(getToken, _listener.getConfig().allowed_methods) == false)
		{
			std::cout << "hola\n";
			_status = 405;
			return ;
		}
	}
	else if (checkMethod(token, _listener.getConfig().allowed_methods) == false)
	{
		std::cout << "hola\n";
		_status = 405;
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
		_status = 400;
		return ;
	}
	this->_headerContent.path = urlDecode(token);
	std::cout << "path: " << token << std::endl;
	token = "";
	while (*begin == ' ')
		++begin;
	for (; begin != end; ++begin)
		token.push_back(*begin);
	if (token != "HTTP/1.0" && token != "HTTP/1.1")
	{
		this->_status = 505;
		return ;
	}
	std::cout << "protocol: " << token << std::endl;
	this->_headerContent.protocol = token;
	// si falla eliminar hasta linea 674
	std::string lower = strToLower(this->_header);
    size_t pos = lower.find("host:");
    if (pos != std::string::npos)
    {
        size_t valStart = pos + 5;
        // avanzar sobre espacios
        while (valStart < this->_header.size() && (this->_header[valStart] == ' ' || this->_header[valStart] == '\t'))
            ++valStart;
        size_t valEnd = this->_header.find("\r\n", valStart);
        if (valEnd == std::string::npos) valEnd = this->_header.size();
        std::string hostVal = this->_header.substr(valStart, valEnd - valStart);
        // quitar posible :port
        size_t colon = hostVal.find(':');
        if (colon != std::string::npos)
            hostVal = hostVal.substr(0, colon);
	// trim simple (C++98 compatible)
	while (!hostVal.empty() && (hostVal[0] == ' ' || hostVal[0] == '\t')) hostVal.erase(hostVal.begin());
	while (!hostVal.empty() && (hostVal[hostVal.size() - 1] == ' ' || hostVal[hostVal.size() - 1] == '\t')) hostVal.erase(hostVal.end()-1);
        this->_headerContent.host = hostVal;
    }
	while (*begin == ' ')
		++begin;
	begin += 2;
	// Parse header lines robustly: split by CRLF, then split each line at the
	// first ':' to obtain name and value. This avoids tokenization issues
	// caused by spaces or ':' inside header values.
	{
		std::string hdrs = this->_header;
		std::vector<std::string> header_lines;
		while (!hdrs.empty())
		{
			size_t p = hdrs.find("\r\n");
			if (p == std::string::npos)
			{
				header_lines.push_back(hdrs);
				hdrs.clear();
			}
			else
			{
				header_lines.push_back(hdrs.substr(0, p));
				hdrs = hdrs.substr(p + 2);
			}
		}
		// Iterate header lines and extract relevant headers
		for (size_t hi = 0; hi < header_lines.size(); ++hi)
		{
			std::string line = header_lines[hi];
			if (line.empty()) continue;
			size_t colonPos = line.find(":");
			if (colonPos == std::string::npos)
			{
				std::cout << "invalid header line: " << line << std::endl;
				this->_status = 400;
				return;
			}
			std::string name = line.substr(0, colonPos);
			std::string value = line.substr(colonPos + 1);
			// trim
			while (!name.empty() && (name[0] == ' ' || name[0] == '\t')) name.erase(name.begin());
			while (!name.empty() && (name[name.size() - 1] == ' ' || name[name.size() - 1] == '\t')) name.erase(name.end()-1);
			while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) value.erase(value.begin());
			while (!value.empty() && (value[value.size() - 1] == ' ' || value[value.size() - 1] == '\t')) value.erase(value.end()-1);
			std::string lname = strToLower(name);
			if (lname == "content-length")
			{
				if (this->_headerContent.isChunked == true)
				{
					this->_status = 404;
					return;
				}
				size_t num = 0;
				std::istringstream ss(value);
				ss >> num;
				checkContentLength(num);
				this->_headerContent.ContentLength = num;
			}
			else if (lname == "transfer-encoding")
			{
				if (strToLower(value).find("chunked") != std::string::npos)
					this->_headerContent.isChunked = true;
			}
		}
		// Debug: dump header summary
		std::cerr << "parseHeader: headers_received=" << this->_header << "\n";
		std::cerr << "parseHeader: ContentLength=" << this->_headerContent.ContentLength << " isChunked=" << this->_headerContent.isChunked << "\n";
	}
}

void	Client::checkContentLength(size_t num)
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
				this->_status = 413;
				return ;
			}
		}
	}
	if (this->_listener.getConfig().client_max_body_size != 0
		&& num > this->_listener.getConfig().client_max_body_size)
		this->_status = 413;
}


void	Client::setPath()
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
				_isLocation = true;
				_location = *it;
				std::cout << "it->path: " << it->path << std::endl;
				if (checkMethod(_headerContent.method, it->allowed_methods) == false)
				{
					std::cout << "method: |" << _headerContent.method << " size methods: " << it->allowed_methods.size() << "|\n";
					for (size_t i = 0; i < it->allowed_methods.size(); ++i)
					{
						std::cout << it->allowed_methods[i] << std::endl;
					}
					_status = 405;
					return ;
				}
				if (!_listener.getConfig().index.empty())
					index = _listener.getConfig().index;
				if (!it->index.empty())
					index = it->index;
				if (_listener.getConfig().isAutoindex == true)
					autoindex = _listener.getConfig().autoindex;
            	//std::cout << "POST CHECK = _listener.getConfig().isAutoindex == true\n";
				if (it->isAutoindex == true)
					autoindex = it->autoindex;
            	//std::cout << "POST AUTO == TRUE\n";
				if (it->isAlias)
				{
					std::string	rest = _headerContent.path.substr(it->path.size());
					_headerContent.path = it->root + rest;
					_headerContent.root = it->root;
					checkPathValidity(_headerContent.path, index, autoindex, it->root);
					return ;
				}
            	//std::cout << "POST CHECK\n";
				if (it->isRoot)
				{
					_headerContent.path = it->root + _headerContent.path;
					_headerContent.root = it->root;
					checkPathValidity(_headerContent.path, index, autoindex, it->root);
            		//std::cout << "POST CHECK is root\n";
					return ;
				}
				_headerContent.path = _listener.getConfig().root + _headerContent.path;
				_headerContent.root = _listener.getConfig().root;
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
			_headerContent.root = _listener.getConfig().root;
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
		_headerContent.root = _listener.getConfig().root;
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

void	Client::checkPathValidity(std::string& path, std::vector<std::string>& index, bool autoindex, const std::string& root)
{
	struct stat st;

	if (stat(path.c_str(), &st) == 0)
	{
    	if (S_ISREG(st.st_mode))
		{
			if (access(path.c_str(), R_OK) != 0)
			{
				_status = 404;
				return ;
			}
			if (!isWithinRoot(path, root))
			{
				//std::cout << "sale por aqui. Path: " << path << ". Root: " << root << std::endl;
				_status = 403;
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
					if (access(joinPath(path, *it).c_str(), F_OK) == 0 || this->_headerContent.method == "POST")
					{
						if (access(joinPath(path, *it).c_str(), R_OK) != 0 && this->_headerContent.method != "POST")
						{
							_status = 404;
							return ;
						}
						if (!isWithinRoot(joinPath(path, *it), root) && this->_headerContent.method != "POST")
						{
							std::cout << "PATH EN IF: " << path << std::endl; 
							_status = 403;
							return ;
						}
						std::cout << "DENTRO DE IF\n";
						if (checkExtention(path, ".py") == true)
							;//comprobar si el path tiene una extension para cgi
						// For POST requests we should not automatically map a directory to
						// its index file; a POST to a directory usually means "create a new
						// resource inside this directory". Mapping to index would make the
						// server overwrite the index file.
						if (this->_headerContent.method != "POST")
							_headerContent.path = joinPath(_headerContent.path, *it);
						// If method is POST, keep _headerContent.path pointing to the
						// directory so later logic can create a new file inside it.
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
				_status = 403;
			}
			return ;
		}
	}
	// If file doesn't exist: for POST we allow creating new resources, so
	// do not set 404 here; caller (e.g., sendResponse) will handle creation.
	if (this->_headerContent.method == "POST")
		return;

	_status = 404;
}

std::string	Client::joinPath(const std::string& a, const std::string& b) // revisar esta funcion
{
    if (a[a.size() - 1] == '/')
	{
    	return a + b;
	}
	return a + "/" + b;
}

void	Client::chunkManagement() //corregir esto
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
        			this->_status = 400;
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
    		this->_status = 400;
    		return ;
		}
		this->_body += this->_chunkLine.substr(0, sublen);
    	this->_chunkLine.erase(0, this->_chunkLen + 2);
    	this->_chunkLen = 0;
	}
}

void	Client::chargeBody()
{
	if (this->_headerContent.method != "POST")
		return ;
	//std::cout << "ENTER chargeBody fd=" << this->_fd << " path=" << this->_headerContent.path << "\n";
	ssize_t bytesRead = recv(this->_fd, this->_buffer, sizeof(this->_buffer), 0); // sustituir el tamaño del buffer a una macro
	//std::cout << "chargeBody: recv returned " << bytesRead << " on fd=" << this->_fd << "\n";
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
		// cliente cerró la conexión: if we already have the expected body length,
		// mark body ready. Otherwise treat as premature EOF and set an error.
		if (!this->_headerContent.isChunked)
		{
			if (this->_headerContent.ContentLength != 0)
			{
				if (this->_body.size() >= this->_headerContent.ContentLength)
				{
					if (this->_body.size() > this->_headerContent.ContentLength)
						this->_body = this->_body.substr(0, this->_headerContent.ContentLength);
					this->_isBodyReady = true;
					this->_request[1] = this->_body;
					std::cout << "chargeBody: EOF but body complete, size=" << this->_body.size() << "\n";
					return;
				}
				// premature EOF
				this->_status = 400;
				return;
			}
			else
			{
				// No Content-Length and connection closed -> accept current body
				this->_isBodyReady = true;
				this->_request[1] = this->_body;
				std::cout << "chargeBody: EOF with no Content-Length, size=" << this->_body.size() << "\n";
				return;
			}
		}
		// For chunked transfer, rely on chunkManagement to set _isBodyReady; treat this as error
		this->_status = 400;
		return;
	}
	else
	{
		// Append raw bytes (binary-safe)
		if (this->_headerContent.isChunked == true)
		{
			// For chunked mode, append the newly-received bytes into the chunkLine
			this->_chunkLine.append(this->_buffer, bytesRead);
			chunkManagement();
			ft_bzero(this->_buffer, sizeof(this->_buffer));
			return ;
		}
		this->_body.append(this->_buffer, bytesRead); // Append raw bytes
		if (this->_headerContent.ContentLength <= this->_body.size())
		{
			if (this->_body.size() > this->_headerContent.ContentLength)
				this->_body = this->_body.substr(0, this->_headerContent.ContentLength);
			this->_isBodyReady = true;
			this->_request[1] = this->_body;
			std::cout << "CHARGE BODY: body ready, size=" << this->_body.size() << "\n";
		}
		ft_bzero(this->_buffer, sizeof(this->_buffer));
	}
}

void	Client::flushResponse()
{
	ssize_t		bytesSent;

	while (!_sendBuffer.empty())
	{
		bytesSent = send(this->_fd, _sendBuffer.c_str(), _sendBuffer.size(), 0);
		std::cerr << "flushResponse: fd=" << this->_fd << " try_send=" << _sendBuffer.size() << " got=" << bytesSent << " errno=" << errno << "\n";
		if (bytesSent > 0)
			_sendBuffer.erase(0, bytesSent);
		else if (bytesSent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			break ;
		else
			break ;	
	}
	_isSent = _sendBuffer.empty();
}

void	Client::handleDelete()
{
	struct stat s;
	// Enforce server root only: do not allow DELETE outside the server's root,
	// even if a location alias points elsewhere.
	std::string root = _listener.getConfig().root;
	if (!isWithinRoot(_headerContent.path, root))
	{
		//std::cout << "EL PUTO ROOT: " << root << "	EL PUTO PATH: " << _headerContent.path << std::endl;
		//std::cout << "EL BICHOOOOOOOOOOOOOOOOOOOOOOOOOOOO" << std::endl;
		_status = 403;
		return;
	}
	if (stat(_headerContent.path.c_str(), &s) != 0)
	{
		if (errno == ENOENT)
			_status = 404;
		else
			_status = 500;
		return;
	}
	if (S_ISDIR(s.st_mode))
	{
		_status = 403;
		return;
	}
	if (std::remove(_headerContent.path.c_str()) != 0)
	{
		if (errno == ENOENT)
			_status = 404;
		else if (errno == EACCES || errno == EPERM)
			_status = 403;
		else
			_status = 500;
		return;
	}
	_status = 204;
}

void	Client::chargeStatusData(const std::map<std::pair<int, int>, std::string>& errorPages)
{
	std::map<std::pair<int, int>, std::string>::const_iterator it = errorPages.begin();
	std::map<std::pair<int, int>, std::string>::const_iterator end = errorPages.end();

	for (; it != end; ++it)
	{
		if (it->first.first != _status)
			continue;
		if (it->first.first != it->first.second)
			_status = it->first.second;
		_headerContent.path = it->second;
		std::string target = it->second;
		while (!target.empty() && (target[0] == ' ' || target[0] == '\t'))
			target.erase(0, 1);
		while (!target.empty() && (target[target.size() - 1] == ' ' || target[target.size() - 1] == '\t'))
			target.erase(target.size() - 1);
		// EXTERNAL URL
		if (target.size() >= 7 && (target.substr(0, 7) == "http://" || (target.size() >= 8 && target.substr(0, 8) == "https://")))
		{
			_redirectLocation = target;
			_hasErrorPageResolved = true;
			return;
		}
		// Local URI or relative path -> resolve to filesystem path using location or server root
		std::string root = _listener.getConfig().root;
		if (_isLocation)
			root = _location.root;
		std::string resolved;
		if (!target.empty() && target[0] == '/')
		{
			// target is a URI path relative to server/location root
			// avoid duplicating slashes
			if (root.size() > 0 && root[root.size() - 1] == '/')
				resolved = root + (target.size() > 1 ? target.substr(1) : std::string());
			else
				resolved = root + target;
		}
		else
		{
			// treat as relative to root
			if (root.size() > 0 && root[root.size() - 1] == '/')
				resolved = root + target;
			else
				resolved = root + "/" + target;
		}
		// validate resolved path
		if (!resolved.empty() && isWithinRoot(resolved, root) && access(resolved.c_str(), R_OK) == 0)
		{
			_errorResolvedPath = resolved;
			_hasErrorPageResolved = true;
			return;
		}
	}
}

void	Client::chargeStatusData(std::map<std::pair<int, int>, std::string>& errorPages)
{
	std::map<std::pair<int, int>, std::string>::const_iterator it = errorPages.begin();
	std::map<std::pair<int, int>, std::string>::const_iterator end = errorPages.end();

	for (; it != end; ++it)
	{
		if (it->first.first != _status)
			continue;
		if (it->first.first != it->first.second)
			_status = it->first.second;
		_headerContent.path = it->second;
		std::string target = it->second;
		while (!target.empty() && (target[0] == ' ' || target[0] == '\t'))
			target.erase(0, 1);
		while (!target.empty() && (target[target.size() - 1] == ' ' || target[target.size() - 1] == '\t'))
			target.erase(target.size() - 1);
		// EXTERNAL URL
		if (target.size() >= 7 && (target.substr(0, 7) == "http://" || (target.size() >= 8 && target.substr(0, 8) == "https://")))
		{
			_redirectLocation = target;
			_hasErrorPageResolved = true;
			return;
		}
		// Local URI or relative path -> resolve to filesystem path using location or server root
		std::string root = _listener.getConfig().root;
		if (_isLocation)
			root = _location.root;
		std::string resolved;
		if (!target.empty() && target[0] == '/')
		{
			// target is a URI path relative to server/location root
			// avoid duplicating slashes
			if (root.size() > 0 && root[root.size() - 1] == '/')
				resolved = root + (target.size() > 1 ? target.substr(1) : std::string());
			else
				resolved = root + target;
		}
		else
		{
			// treat as relative to root
			if (root.size() > 0 && root[root.size() - 1] == '/')
				resolved = root + target;
			else
				resolved = root + "/" + target;
		}
		// validate resolved path
		if (!resolved.empty() && isWithinRoot(resolved, root) && access(resolved.c_str(), R_OK) == 0)
		{
			_errorResolvedPath = resolved;
			_hasErrorPageResolved = true;
			return;
		}
	}
}

std::string	Client::chargeDefaultErrorPage()
{
	std::string			body;
	std::stringstream	bs;
	std::stringstream	hs;

	bs << "<html><head><title>"
		<< _status
		<< " "
		<< reasonPhrase(_status)
		<< "</title></head><body><center><h1>"
		<< _status
		<< " "
		<<  reasonPhrase(_status)
		<< "</h1></center><hr><center>webserv</center></body></html>";
	return bs.str();
}

void	Client::handleErrors()
{
	if (_isLocation && _location.areErrorPages)
	{
		chargeStatusData(_location.error_pages);
		return ;
	}
	if (_listener.getConfig().areErrorPages)
	{
		chargeStatusData(_listener.getConfig().error_pages);
		return ;
	}
	//if (this->_status >= 300 && this->_status < 600)
	//	chargeDefaultErrorPage();
	/*if (this->_status >= 300 && this->_status < 600)
	{
		std::ostringstream hs;
		hs << this->_headerContent.protocol << " " << this->_status << " " << reasonPhrase(this->_status) << "\r\n";
		hs << "Connection: close\r\n";
		hs << "Content-Length: 0\r\n\r\n";
		_sendBuffer += hs.str();
		_isSent = true;
		flushResponse();
	}*/
}

void	Client::sendResponse()
{	
	std::cout << "ENTER sendResponse for fd " << this->_fd << " method=" << this->_headerContent.method << "\n";
	if (_sendBuffer.empty())
	{
		// If an error was already set before preparing response, send that error code
		handleErrors();

		// If chargeStatusData resolved an external redirect, send it immediately
		if (_hasErrorPageResolved && !_redirectLocation.empty())
		{
			int redirectStatus = _status;
			if (!(redirectStatus >= 300 && redirectStatus < 400))
				redirectStatus = 302; // fallback to temporary redirect
			std::ostringstream hs;
			hs << this->_headerContent.protocol << " " << redirectStatus << " " << reasonPhrase(redirectStatus) << "\r\n";
			hs << "Location: " << _redirectLocation << "\r\n";
			hs << "Connection: close\r\n";
			hs << "Content-Length: 0\r\n\r\n";
			_sendBuffer += hs.str();
			_isSent = true;
			flushResponse();
			return;
		}
		

		// Handle DELETE specially: do not load the file content before attempting to delete it
		if (this->_headerContent.method == "DELETE")
		{
			handleDelete();
			handleErrors(); // send error if delete failed, or 204 No Content if succeeded
			// Success: 204 No Content
			std::ostringstream hs;
			hs << this->_headerContent.protocol << " 204 " << reasonPhrase(204) << "\r\n";
			hs << "Connection: close\r\n";
			hs << "Content-Length: 0\r\n\r\n";
			_sendBuffer += hs.str();
			_isSent = true;
			flushResponse();
			return;
		}

		if (this->_headerContent.method == "POST")
		{
			std::cout << "sendResponse: POST path=" << this->_headerContent.path << " ContentLength=" << this->_headerContent.ContentLength << " body.size=" << this->_body.size() << "\n";
			std::string serverRoot = _listener.getConfig().root;
			if (!isWithinRoot(_headerContent.path, serverRoot))
			{
				this->_status = 403;
        		// prepare error response (ya lo manejas más arriba)
        		std::ostringstream hs;
        		hs << this->_headerContent.protocol << " " << this->_status << " " << reasonPhrase(this->_status) << "\r\n";
        		hs << "Connection: close\r\n";
        		hs << "Content-Length: 0\r\n\r\n";
        		_sendBuffer += hs.str();
        		_isSent = true;
        		flushResponse();
        		return;
			}
			struct stat st;
			bool existed = (stat(this->_headerContent.path.c_str(), &st) == 0);
			if (existed && S_ISDIR(st.st_mode))
			{
				_status = 403;
				return;
			}
			std::string ext = getExtension(_headerContent.path);
			std::map<std::string,std::string> cgiMap;
			if (_isLocation)
				cgiMap = _location.cgi;
			else
				cgiMap = _listener.getConfig().cgi; //location.cgi
			if (cgiMap.count(ext))
    		{
    		    if (!isCgiRunning())
				{
					if (!startCgiNonBlocking(this->_headerContent.path, cgiMap[ext]))
					{
						this->_status = 500;
						return;
					}
				}
    		    // response will be produced when CGI finishes (finalizeCgiIfDone)
    		    return;
    		}
			int	fd = open(this->_headerContent.path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
			if (fd < 0)
			{
				std::cerr << "sendResponse: open failed for " << this->_headerContent.path << " errno=" << errno << " (" << strerror(errno) << ")\n";
			}
			if (fd < 0)
			{
				if (errno == EACCES || errno == EPERM) this->_status = 403;
        		else if (errno == ENOENT) this->_status = 404;
        		else this->_status = 500;
				return ;
			}
			ssize_t to_write = _body.size();
    		const char* buf = _body.c_str();
    		ssize_t written = 0;
			while (to_write > 0)
			{
				ssize_t w = write(fd, buf + written, to_write);
        		if (w <= 0)
        		{
        		    close(fd);
        		    this->_status = 500;
        		    return;
        		}
        		written += w;
        		to_write -= w;
			}
			close (fd);
			int status;
			if (existed)
				status = 200;
			else
				status = 201;
			// Build a Location header using the request path (strip server root)
			std::string requestUri = this->_headerContent.path;
			if (requestUri.find(serverRoot) == 0)
				requestUri = requestUri.substr(serverRoot.size());
			if (requestUri.empty())
				requestUri = "/";
			std::ostringstream hs;
			hs << this->_headerContent.protocol << " " << status << " " << reasonPhrase(status) << "\r\n";
			if (!existed) hs << "Location: " << requestUri << "\r\n";
			hs << "Connection: close\r\n";
			hs << "Content-Length: 0\r\n\r\n";
			_sendBuffer += hs.str();
			_isSent = true;
			flushResponse();
			return;
		}
		std::string body;
		// If path is a directory and autoindex response was requested, generate listing
		struct stat st;
		if (stat(this->_headerContent.path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && _headerContent.isAutoindexResponse)
		{
			try {
				std::string requestPath = this->_headerContent.path;
				std::string root = _listener.getConfig().root;
				if (requestPath.find(root) == 0)
					requestPath = requestPath.substr(root.size());
				if (requestPath.empty()) requestPath = "/";
				body = generateDirectoryListing(this->_headerContent.path, requestPath);
			} catch (const std::bad_alloc& e) {
				std::cerr << "sendResponse: std::bad_alloc while preparing autoindex for " << this->_headerContent.path << "\n";
				this->_status = 500;
				body.clear();
			}
		}
		else
		{
			// If chargeStatusData resolved a custom error file, use it
			if (_hasErrorPageResolved && !_errorResolvedPath.empty())
				body = loadContent(_errorResolvedPath);
			else
				body.clear();

			// Check CGI for GET/HEAD: if the requested path corresponds to a CGI script
			// Prefer using the CGI-provided body (set by finalizeCgiIfDone). Only start
			// the CGI if we don't already have a body and the CGI is not running.
			if (this->_headerContent.method == "GET" || this->_headerContent.method == "HEAD")
			{
				std::string ext = getExtension(_headerContent.path);
				std::map<std::string,std::string> cgiMap;
				if (_isLocation)
					cgiMap = _location.cgi;
				else
					cgiMap = _listener.getConfig().cgi;
				if (cgiMap.count(ext))
				{
					// If CGI already produced a body, use it
					if (this->_isBodyReady && !this->_body.empty())
					{
						body = this->_body;
					}
					else
					{
						// No body yet: start CGI (if not running) and wait for finalizeCgiIfDone
						if (!isCgiRunning())
						{
							if (!startCgiNonBlocking(this->_headerContent.path, cgiMap[ext]))
							{
								this->_status = 500;
								return;
							}
						}
						// Response will be produced by finalizeCgiIfDone()
						return;
					}
				}
			}

			// If not CGI or CGI produced body, and body still empty, load file content
			if (body.empty())
			{
				body = loadContent(this->_headerContent.path);
			}
		}

		// Only set default 200 if status indicates success-range
		if (!(_status >= 300 && _status < 600))
		{
			_status = 200;
		}
		else
		{
			body = chargeDefaultErrorPage();
		}
		std::ostringstream hs;
		hs << this->_headerContent.protocol << " " << _status << " " << reasonPhrase(_status) << "\r\n";
		hs << "Content-Length: " << body.size() << "\r\n";
		// Prefer CGI-specified Content-Type when available
		if (!this->_cgiContentType.empty())
			hs << "Content-Type: " << this->_cgiContentType << "\r\n";
		else if (this->_headerContent.isAutoindexResponse || (_status > 299 && _status < 600))
			hs << "Content-Type: text/html\r\n";
		else
			hs << "Content-Type: " << getMimeType(this->_headerContent.path) << "\r\n";
		hs << "Connection: close\r\n";
		hs << "\r\n";
		_sendBuffer += hs.str();
		
		std::cout << "body: " << body << std::endl;
		if (this->_headerContent.method != "HEAD")   
			_sendBuffer += body;
	}
	// Do not print binary response to stdout (can be very large and block logs)
	flushResponse();
}

/*std::string	Client::loadContent(const std::string& filename) const
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

std::string	Client::loadContent(const std::string& filename) const
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


bool	Client::getIsSent() const { return _isSent; }
