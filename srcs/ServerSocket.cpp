#include "ServerSocket.hpp"

/*ServerSocket::ServerSocket(const char* port)
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
*/
/*int	ServerSocket::get_fd(){ return _fd; }*/

ServerSocket::~ServerSocket()
{
	closeAll();
}

ServerSocket::ServerSocket()
{}

static void	restartListenFd(int& listen_fd)
{
	close(listen_fd);
	listen_fd = -1;
}

bool	ServerSocket::createListeners(std::vector<Server>& servers)
{
	/*
	1. createListeners — varios servidores comparten el mismo listen_fd si tienen el mismo ip:port
	Cuando un ip:port ya está en _created, haces continue y no llamas a servers[i].setFd(...). Eso significa que el segundo servidor que escucha en el mismo puerto se queda con _fd sin inicializar (-1 si aplicaste el fix anterior). Necesitas buscar el fd ya creado y asignárselo:
	cppif (_created.find(key) != _created.end())
	{
    	// Buscar el fd ya creado para este ip:port y asignarlo
    	for (size_t k = 0; k < _listeners.size(); ++k) {
        // necesitarías un map<pair, int> en lugar de un set para hacer esto bien
    }
    	continue;
	}
	La solución limpia es cambiar _created de std::set a std::map<std::pair<std::string,std::string>, int> que mapee ip:port → fd.*/
	int		on = 1;
	bool	any = false;
	for (std::vector<Server>::size_type i = 0; i < servers.size(); i++)
	{
		const ServerConfig& cfg = servers[i].getConfig();
		for (std::multimap<std::string, std::string>::const_iterator it = cfg.listen.begin(); it != cfg.listen.end(); ++it)
		{
			std::string ip = it->first;
			std::string	port = it->second;
			std::pair<std::string, std::string> key(ip, port);
			if (_created.find(key) != _created.end())
				continue;
			struct addrinfo		hints;
			struct addrinfo*	res = NULL;
			ft_bzero(&hints, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_PASSIVE;
			int	gai = getaddrinfo(ip.c_str(), port.c_str(), &hints, &res);
			if (gai != 0)
			{
				std::cerr << "getaddrinfo: " << gai_strerror(gai) << " for " <<
					ip << ":" << port << "\n";
				continue;
			}
			struct addrinfo* rp = res;
			int	listen_fd = -1;
			for (; rp != NULL; rp = rp->ai_next)
			{
				listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
				if (listen_fd < 0)
				{
					continue;
				}
				if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
				{
					restartListenFd(listen_fd);
					continue;
				}
				int	flags = fcntl(listen_fd, F_GETFL, 0);
				if (flags == -1)
					flags = 0;
				if (fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK) == -1)
				{
					restartListenFd(listen_fd);
					continue;
				}
				if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) < 0)
				{
					std::cerr << "No se pudo crear listener: " << ip << ":" << port
         			<< " -> " << strerror(errno) << "\n";
					restartListenFd(listen_fd);
					continue;
				}
				if (listen(listen_fd, SOMAXCONN) < 0)
				{
					restartListenFd(listen_fd);
					continue;
				}
				servers[i].setFd(listen_fd);
				break;
			}
			freeaddrinfo(res);
			if (listen_fd < 0)
			{
				std::cerr << "No se pudo crear listener: " << ip << ":" << port << "\n";
				continue;
			}
			_listeners.push_back(listen_fd);
			struct pollfd p;
			p.fd = listen_fd;
			p.events = POLLIN;
			p.revents = 0;
			_pollfds.push_back(p);
			_created.insert(key);
			any = true;
			std::cout << "Listening on " << ip << ":" << port << " fd=" << listen_fd << "\n";
		}
	}
	return any;	
}

int	ServerSocket::acceptNewClient(int listen_fd)
{
	struct sockaddr_storage	peer;
	socklen_t				plen = sizeof(peer);
	int						client_fd = accept(listen_fd, (struct sockaddr*)&peer, &plen);
	if (client_fd < 0)
	{
		/*if (errno == EAGAIN || errno == EWOULDBLOCK)
			return -1;*/
		std::cerr << "accept error: " << strerror(errno) << std::endl;
		return -1;
	}
	int	flags = fcntl(client_fd, F_GETFL, 0);
	if (flags == -1)
	{
		std::cerr << "fcntl F_GETFL: " << strerror(errno) << std::endl;
		flags = 0;
	}
	if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		std::cerr << "fcntl F_SETFL: " << strerror(errno) << std::endl;
		close(client_fd);
		return -1;
	}
	return client_fd;
}

const std::vector<int>&	ServerSocket::getListeners() const
{
	return _listeners;
}

const std::vector<struct pollfd>& ServerSocket::getPollfds() const
{
	return _pollfds;
}

void	ServerSocket::closeAll()
{
	for (std::vector<int>::size_type i = 0; i < _listeners.size(); ++i)
		close(_listeners[i]);
	_listeners.clear();
	_pollfds.clear();
	_created.clear();
}

bool	ServerSocket::isListener(int fd)
{
	for (size_t i = 0; i < _listeners.size(); ++i)
	{
		if (_listeners[i] == fd)
			return true;
	}
	return false;
}