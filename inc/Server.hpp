#ifndef SERVER_HPP
# define SERVER_HPP

# include <map>
# include <string>
# include <vector>

struct LocationConfig
{
	std::string							path;
	std::string							root;
	//std::string							alias;
	std::vector<std::string>			index;
	std::vector<std::string>			allowed_methods;
    std::map<std::string, std::string>	cgi;
	std::map<int, std::string>			error_pages;
	bool								autoindex;
	bool								isAutoindex;
	//bool								isRootOrAlias;
	bool								isRoot;
	bool								isAlias;
};


struct ServerConfig
{
    std::multimap<std::string, std::string>	listen;
    std::map<std::string, std::string>		cgi;
    std::vector<std::string>				index;
	bool									autoindex;
	std::map<int, std::string>				error_pages;
	std::string								root;
	//std::string								alias;
	std::string								server_name;
	size_t									client_max_body_size;
	std::vector<LocationConfig>				locations;
	std::vector<std::string>				allowed_methods;
	bool									isAutoindex;
	bool									isRoot;
	bool									isAlias;
	//bool									isRootOrAlias; // if true
};


class Server
{
private:
    //std::map<std::string, std::string>   _listen;
    ServerConfig	_config;
	int				_fd;
public:
    Server();
    ~Server();

	void	addListen(std::string& ip, std::string& port);
	void	addCgi(std::string& ext, std::string &path);
	void	addIndex(std::string& name);
	void	setAutoindex(bool aI);
	void	setRoot(std::string& r, int n);
	void	setServerName(std::string& sn);
	void	setClientMaxBodySize(long cmbs, char c);
	void	addErrorPage(int code, std::string& uri);
	void	addLocation(LocationConfig& loc);
	void	setAllowedMethods(const std::vector<std::string>& m);
	void	setFd(int fd);
	int		getFd() const;

	const	ServerConfig&	getConfig()const;
};

#endif