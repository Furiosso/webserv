#include "ServerSocket.hpp"
#include <poll.h>
#include <map>
#include <vector>
#include <string>
#include "Parser.hpp"
#include "Server.hpp"
#include "RequestHandler.hpp"


//AUXILIAR PARA PRINTEO DE SERVER
static void printVector(const std::vector<std::string>& v)
{
    for (std::vector<std::string>::size_type i = 0; i < v.size(); ++i)
    {
        std::cout << v[i];
        if (i + 1 < v.size()) std::cout << ", ";
    }
}

int main (int argc, char** argv, char** env)
{
    (void)argv;
    (void)env;

    if (argc != 2)
    {
        // include error message here
        return 1;
    }
    try
    {  
        std::vector<Server>         servers;
        Parser parser(argv[1], servers);
        for (std::vector<Server>::size_type i = 0; i < servers.size(); ++i)
        {
            const ServerConfig& cfg = servers[i].getConfig();
            std::cout << "=== Server #" << i << " ===\n";

            std::cout << "Listen:\n";
            for (std::multimap<std::string, std::string>::const_iterator it = cfg.listen.begin();
                 it != cfg.listen.end(); ++it)
                std::cout << "  " << it->first << ":" << it->second << "\n";

            std::cout << "CGI:\n";
            for (std::map<std::string, std::string>::const_iterator it = cfg.cgi.begin();
                 it != cfg.cgi.end(); ++it)
                std::cout << "  " << it->first << " -> " << it->second << "\n";

            std::cout << "Index: ";
            printVector(cfg.index);
            std::cout << "\n";

            std::cout << std::boolalpha;
            std::cout << "Autoindex: " << cfg.autoindex << "\n";
            std::cout << "Root: " << cfg.root << "\n";
            std::cout << "Alias: " << cfg.alias << "\n";
            std::cout << "Server name: " << cfg.server_name << "\n";
            std::cout << "Client max body size: " << cfg.client_max_body_size << "\n";

            std::cout << "Allowed methods: ";
            printVector(cfg.allowed_methods);
            std::cout << "\n";

            std::cout << "Error pages:\n";
            for (std::map<int, std::string>::const_iterator it = cfg.error_pages.begin();
                 it != cfg.error_pages.end(); ++it)
                std::cout << "  " << it->first << " -> " << it->second << "\n";

            std::cout << "isRootOrAlias: " << cfg.isRootOrAlias << "\n";

            std::cout << "Locations (" << cfg.locations.size() << "):\n";
            for (std::vector<LocationConfig>::size_type j = 0; j < cfg.locations.size(); ++j)
            {
                const LocationConfig& loc = cfg.locations[j];
                std::cout << "  -- Location #" << j << " --\n";
                std::cout << "    Path: " << loc.path << "\n";
                std::cout << "    Root: " << loc.root << "\n";
                std::cout << "    Alias: " << loc.alias << "\n";
                std::cout << "    Index: ";
                printVector(loc.index);
                std::cout << "\n";
                std::cout << "    Allowed methods: ";
                printVector(loc.allowed_methods);
                std::cout << "\n";
                std::cout << "    CGI:\n";
                for (std::map<std::string, std::string>::const_iterator it = loc.cgi.begin();
                     it != loc.cgi.end(); ++it)
                    std::cout << "      " << it->first << " -> " << it->second << "\n";
                std::cout << "    Error pages:\n";
                for (std::map<int, std::string>::const_iterator it = loc.error_pages.begin();
                     it != loc.error_pages.end(); ++it)
                    std::cout << "      " << it->first << " -> " << it->second << "\n";
                std::cout << "    Autoindex: " << loc.autoindex << "\n";
                std::cout << "    isRootOrAlias: " << loc.isRootOrAlias << "\n";
            }

            std::cout << std::noboolalpha << "======================\n\n";
        }
        ServerSocket sockman;
        if (!sockman.createListeners(servers))
        {
            std::cerr << "No listeners created\n";
            return 1;
        }
        std::vector<struct pollfd> pollfds = sockman.getPollfds();
		std::vector<RequestHandler> clients;
        while (1)
        {
            nfds_t  nfds = static_cast<nfds_t>(pollfds.size());
		    int ret = poll(pollfds.data(), nfds, -1);
            if (ret < 0)
            {
                std::cerr << "Poll not ready: " << strerror(errno) << "\n";
                break;
            }
            for(std::vector<struct pollfd>::size_type i = 0; i < pollfds.size(); ++i)
            {
				if (!(pollfds[i].revents & POLLIN) && !(pollfds[i].revents & POLLOUT))
				{
					close(pollfds[i].fd);
					std::swap(pollfds[i], pollfds.back());
					pollfds.pop_back();
					continue;
				}
				if(pollfds[i].revents & POLLIN)
				{
                    int fd = pollfds[i].fd;
                    bool    is_listen = sockman.isListener(fd);
					if (is_listen)
                    {
                        int client_fd = sockman.acceptNewClient(fd);
                        if (client_fd > 0)
                        {
                            struct pollfd newp;
                            newp.fd = client_fd;
                            newp.events = POLLIN;
                            newp.revents = 0;
                            pollfds.push_back(newp);
							for (size_t j = 0; j < servers.size(); ++j)
							{
								if (servers[j].getFd() == fd)
								{
									RequestHandler client(servers[j], client_fd);
									clients.push_back(client);
									break;
								}
							}
                        }
                    }
					else
					{
						//manageRequest(pollfds[i].fd);
					}
				}
				if (pollfds[i].revents & POLLOUT)
				{
					//sendResponse(pollfds[i].fd);
				}
            }
        }
        /*for (size_t i = 0; i < )
        {}*/
        /*ServerSocket prueba("80");
        struct pollfd* fds;
        fds->fd = prueba.get_fd();
        fds->events = POLLIN;
        int nfds = 1;*/
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    /*while (true)
    {
        if (poll(fds, 1, -1))
        {
            for (int i = 0; i < nfds; ++i)
            {
                if (fds[i].revents & POLLIN)
                {
                    if (fds[i].fd == prueba.get_fd())
                    {
                        int client_fd = accept(fds->fd, NULL, NULL);
                        fds[nfds].fd = client_fd;
                        fds[nfds].events = POLLIN;
                        ++nfds;
                    }
                    else{
                        char    buffer[4096];
                        ssize_t n = recv(fds[i].fd, buffer, sizeof(buffer), 0);
                        if (n <= 0)
                            close(fds[i].fd);
                    }               
                }
            }
        }
    }*/
}



//PRINTEO SERVER.. METER EN TRY DESPUES DE INICIARLOS

//borrar abajo
        /*for (std::vector<Server>::size_type i = 0; i < servers.size(); ++i)
        {
            const ServerConfig& cfg = servers[i].getConfig();
            std::cout << "=== Server #" << i << " ===\n";

            std::cout << "Listen:\n";
            for (std::multimap<std::string, std::string>::const_iterator it = cfg.listen.begin();
                 it != cfg.listen.end(); ++it)
                std::cout << "  " << it->first << ":" << it->second << "\n";

            std::cout << "CGI:\n";
            for (std::map<std::string, std::string>::const_iterator it = cfg.cgi.begin();
                 it != cfg.cgi.end(); ++it)
                std::cout << "  " << it->first << " -> " << it->second << "\n";

            std::cout << "Index: ";
            printVector(cfg.index);
            std::cout << "\n";

            std::cout << std::boolalpha;
            std::cout << "Autoindex: " << cfg.autoindex << "\n";
            std::cout << "Root: " << cfg.root << "\n";
            std::cout << "Alias: " << cfg.alias << "\n";
            std::cout << "Server name: " << cfg.server_name << "\n";
            std::cout << "Client max body size: " << cfg.client_max_body_size << "\n";

            std::cout << "Allowed methods: ";
            printVector(cfg.allowed_methods);
            std::cout << "\n";

            std::cout << "Error pages:\n";
            for (std::map<int, std::string>::const_iterator it = cfg.error_pages.begin();
                 it != cfg.error_pages.end(); ++it)
                std::cout << "  " << it->first << " -> " << it->second << "\n";

            std::cout << "isRootOrAlias: " << cfg.isRootOrAlias << "\n";

            std::cout << "Locations (" << cfg.locations.size() << "):\n";
            for (std::vector<LocationConfig>::size_type j = 0; j < cfg.locations.size(); ++j)
            {
                const LocationConfig& loc = cfg.locations[j];
                std::cout << "  -- Location #" << j << " --\n";
                std::cout << "    Path: " << loc.path << "\n";
                std::cout << "    Root: " << loc.root << "\n";
                std::cout << "    Alias: " << loc.alias << "\n";
                std::cout << "    Index: ";
                printVector(loc.index);
                std::cout << "\n";
                std::cout << "    Allowed methods: ";
                printVector(loc.allowed_methods);
                std::cout << "\n";
                std::cout << "    CGI:\n";
                for (std::map<std::string, std::string>::const_iterator it = loc.cgi.begin();
                     it != loc.cgi.end(); ++it)
                    std::cout << "      " << it->first << " -> " << it->second << "\n";
                std::cout << "    Error pages:\n";
                for (std::map<int, std::string>::const_iterator it = loc.error_pages.begin();
                     it != loc.error_pages.end(); ++it)
                    std::cout << "      " << it->first << " -> " << it->second << "\n";
                std::cout << "    Autoindex: " << loc.autoindex << "\n";
                std::cout << "    isRootOrAlias: " << loc.isRootOrAlias << "\n";
            }

            std::cout << std::noboolalpha << "======================\n\n";
        }*/