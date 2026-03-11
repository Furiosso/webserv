#include "RequestHandler.hpp"

RequestHandler::RequestHandler(Server& listener, int fd) : _listener(listener), _fd(fd), _error(0), _body("")
{
	_headerContent.isChunked = false;
}

RequestHandler::~RequestHandler() {}

void RequestHandler::setClientFd(int fd) { this->_fd = fd; }

int RequestHandler::getClientFd() const { return this->_fd; }

void RequestHandler::chargeHeader(int fd, size_t maxBodySize)
{
	ssize_t bytesRead = recv(fd, this->_buffer, sizeof(this->_buffer) - 1, 0); // sustituir el tamaño del buffer a una macro
	if (bytesRead < 0)
	{
		std::cerr << "Error reading from socket: " << strerror(errno) << std::endl;
		throw std::exception();
	}
	else if (bytesRead == 0)
		throw std::exception(); // cambiarlo por otra cosa
	else
	{
		this->_buffer[bytesRead] = '\0'; // Null-terminate the buffer
		this->_header += this->_buffer; // Append to the request string
		ft_bzero(this->_buffer, sizeof(this->_buffer));
		// Check if the request exceeds the maximum body size
		if (this->_header.size() > maxBodySize)
		{
			this->_error = 413; // Payload Too Large
			return ;
		}
		if (this->_header.find("\r\n\r\n") != std::string::npos) // End of headers
		{
			size_t headerEnd = this->_header.find("\r\n\r\n");
			this->_header = this->_header.substr(0, headerEnd);
			this->parseHeader();
			this->setPath();
			this->_request[0] = this->_header.substr(0, this->_header.find("\r\n")); // Request line
			if (headerEnd + 4 < this->_header.size())
				this->_body = this->_header.substr(headerEnd + 4);
		}
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
	this->_header = this->_header.substr(headerEnd + 2, this->_header.size()); //this->_header = this->_header.substr(headerEnd + 2);
	if (wordCounter(line, ' ') != 3)
	{
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
		this->_error = 505;
		return ;
	}
	this->_headerContent.method = token;
	token = "";
	while (*begin == ' ')
		++begin;
	for (; begin != end; ++begin)
	{
		if (*begin == ' ')
			break;
		token.push_back(*begin);
	}
	//comprobar que el path exista
	this->_headerContent.path = token;
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
	this->_headerContent.protocol = token;
	while (*begin == ' ')
		++begin;
	begin += 2;
	while (this->_header.find("\r\n") != std::string::npos)
	{
		line = this->_header.substr(0, this->_header.find("\r\n")); 
		if (wordCounter(line, ':') != 2)
			//return error;
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
					token = "";
			}
			++begin;
		}
		this->_header = this->_header.substr(this->_header.find("\r\n"), this->_header.size());
	}
	std::vector<std::string>::iterator	vegin = tokens.begin();
	std::vector<std::string>::iterator	vend = tokens.end();
	std::stringstream					ss;
	size_t								num;
	for (; vegin != vend; ++vegin)
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

void	RequestHandler::setPath()
{
	
}
