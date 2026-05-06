#include "ServerSocket.hpp"


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