#include "Server.hpp"

Server::Server()
{
	//_config.listen.insert(std::pair<std::string, std::string>("127.0.0.1", "80"));
	//si a la hora de transmitir la informacion a getaddrinfo listen.size es igual a 0 intorducir en ese momento los valores por defecto
	_config.autoindex = false;
	_config.allowed_methods.push_back("GET");
	_config.allowed_methods.push_back("POST");
	_config.allowed_methods.push_back("DELETE");
	//_config.isRootOrAlias = false;
	_config.root = "./www/jfercode";
	_config.isRoot = false;
	_config.isAlias = false;
	_config.isAutoindex = false;
	_config.areErrorPages = false;
	_config.client_max_body_size = 1048576;
}

Server::~Server(){}

void	Server::addListen(std::string& ip, std::string& port)
{
	_config.listen.insert(std::pair<std::string, std::string>(ip, port));
}

void	Server::addCgi(std::string& ext, std::string& path)
{
	_config.cgi.insert(std::pair<std::string, std::string>(ext, path));
}

void	Server::addIndex(std::string& name)
{
	_config.index.push_back(name);
}

void	Server::setAutoindex(bool aI)
{
	_config.autoindex = aI;
	_config.isAutoindex = true;
}

void	Server::setRoot(std::string& r, int n)
{
	_config.root = r;
	if (n == 0)
		_config.isRoot = true;
	if (n == 1)
		_config.isAlias = true;
}

void	Server::setServerName(std::string& sn)
{
	_config.server_name = sn;
}

void	Server::setClientMaxBodySize(long cmbs, char c)
{
	if (c == 'K' || c == 'k')
		cmbs *= 1024;
	if (c == 'M' || c == 'm')
		cmbs *= 1048576;
	if (c == 'G' || c == 'g')
		cmbs *= 1073741824;
	_config.client_max_body_size = cmbs;
}

void	Server::addErrorPage(std::pair<int, int> codes, std::string& uri)
{
	_config.error_pages.insert(std::pair<std::pair<int, int>, std::string>(codes, uri));
	_config.areErrorPages = true;
}

void	Server::addLocation(LocationConfig& loc)
{
	_config.locations.push_back(loc);
}

void	Server::setAllowedMethods(const std::vector<std::string>& m)
{
	_config.allowed_methods = m;
}

ServerConfig&	Server::getConfig()
{
	return _config;
	//volver a la version anterior en el que devuelve una referencia constante y el método es constante una vez esté correctamente configurada el método charge status data en la clase Client
}

void	Server::setFd(int fd)
{
	_fd = fd;
}

int		Server::getFd() const
{
	return _fd;
}