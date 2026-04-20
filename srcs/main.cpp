#include "ServerSocket.hpp"
#include <poll.h>
#include <map>
#include <vector>
#include <string>
#include "Parser.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include <typeinfo>


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
    (void)env;

    if (argc != 2)
    {
        std::cerr << "Invalid arguments\n";
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
            //std::cout << "Alias: " << cfg.alias << "\n";
            std::cout << "Server name: " << cfg.server_name << "\n";
            std::cout << "Client max body size: " << cfg.client_max_body_size << "\n";

            std::cout << "Allowed methods: ";
            printVector(cfg.allowed_methods);
            std::cout << "\n";

            std::cout << "Error pages:\n";
            for (std::map<std::pair<int, int>, std::string>::const_iterator it = cfg.error_pages.begin();
                 it != cfg.error_pages.end(); ++it)
                std::cout << "  " << it->first.first << " | " << it->first.second << " -> " << it->second << "\n";

            //std::cout << "isRootOrAlias: " << cfg.isRootOrAlias << "\n";

            std::cout << "Locations (" << cfg.locations.size() << "):\n";
            for (std::vector<LocationConfig>::size_type j = 0; j < cfg.locations.size(); ++j)
            {
                const LocationConfig& loc = cfg.locations[j];
                std::cout << "  -- Location #" << j << " --\n";
                std::cout << "    Path: " << loc.path << "\n";
                std::cout << "    Root: " << loc.root << "\n";
                //std::cout << "    Alias: " << loc.alias << "\n";
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
                for (std::map<std::pair<int, int>, std::string>::const_iterator it = loc.error_pages.begin(); it != loc.error_pages.end(); ++it)
                    std::cout << "      " << it->first.first << " | " << it->first.second << " -> " << it->second << "\n";
                std::cout << "    Autoindex: " << loc.autoindex << "\n";
                //std::cout << "    isRootOrAlias: " << loc.isRootOrAlias << "\n";
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
        std::vector<Client> clients;
        // Map CGI fds (in_fd/out_fd) to client index in `clients` vector
        std::map<int, size_t> cgiFdToClientIdx;

        // helper functions (C++98 compatible)
        struct CgiHelpers {
            static void registerCgiFd(std::vector<struct pollfd>& pollfds, std::map<int, size_t>& map, int fd, size_t clientIdx, short events) {
                if (fd < 0) return;
        struct pollfd p; p.fd = fd; p.events = events; p.revents = 0; pollfds.push_back(p);
        map[fd] = clientIdx;
        std::cerr << "registerCgiFd: registered fd=" << fd << " for clientIdx=" << clientIdx << " events=" << events << "\n";
            }
            static void unregisterCgiFds(std::vector<struct pollfd>& pollfds, std::map<int, size_t>& map, int infd, int outfd) {
                if (infd >= 0) map.erase(infd);
                if (outfd >= 0) map.erase(outfd);
                for (size_t k = 0; k < pollfds.size(); ) {
                    if (pollfds[k].fd == infd || pollfds[k].fd == outfd) {
            std::cerr << "unregisterCgiFds: removing fd=" << pollfds[k].fd << "\n";
            close(pollfds[k].fd);
                        std::swap(pollfds[k], pollfds.back());
                        pollfds.pop_back();
                    } else ++k;
                }
            }
        };
        while (1)
        {
            nfds_t  nfds = static_cast<nfds_t>(pollfds.size());
		    int ret = poll(pollfds.data(), nfds, -1);
            if (ret < 0)
            {
                std::cerr << "Poll not ready: " << strerror(errno) << "\n";
                break;
            }
            // Before iterating events, register any newly-started CGI fds from clients
            for (size_t ci = 0; ci < clients.size(); ++ci) {
                int inFd = clients[ci].getCgiInFd();
                int outFd = clients[ci].getCgiOutFd();
                if (inFd >= 0 && cgiFdToClientIdx.count(inFd) == 0) {
                    CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, inFd, ci, POLLOUT);
                }
                if (outFd >= 0 && cgiFdToClientIdx.count(outFd) == 0) {
                    CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, outFd, ci, POLLIN);
                }
            }

            for(std::vector<struct pollfd>::size_type i = 0; i < pollfds.size(); ++i)
            {
                // Debug: show which fd has revents set
                
                //comprobar signals
                if (pollfds[i].revents & POLLHUP)
				{
                    //std::cout << "sus muertos\n";
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
                        std::cout << "listen fd: " << fd << " | client fd: " << client_fd << "\n";
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
                                    std::cout << "fd server = " << fd << std::endl;
                                    try {
                                        clients.push_back(Client(servers[j], client_fd));
                                        clients.back().setEnv(env);
                                    }
                                    catch (const std::exception& e) {
                                        std::cerr << "Failed to store client: " << e.what() << std::endl;
                                        close(client_fd);
                                        continue;
                                    }
                                    std::cout << "cualquier tonteria AQUI\n";
                                    // charge header on the in-place constructed client
                                    clients.back().chargeHeader();
									//Si peta borrar hasta linea 210-211
									std::string reqHost = clients.back().getHeaderHost(); // requiere getHeaderHost()
    								if (!reqHost.empty()) {
    								    // Buscar entre servers el que tenga server_name == reqHost
    								    bool found = false;
    								    for (size_t k = 0; k < servers.size(); ++k) {
    								        const ServerConfig& cfg = servers[k].getConfig();
    								        // Sólo considerar servidores que escuchan en el mismo listener fd
    								        // (el server por defecto para este client es servers[j]; aquí comprobamos coincidencias globales)
    								        if (cfg.server_name == reqHost) {
    								            clients.back().setListener(servers[k]); // requiere setListener(const Server&)
    								            found = true;
    								            break;
    								        }
    								    }
    								    // si no se encontró, se mantiene el server por defecto asignado al cliente
    								}
                                    // After parsing headers, try to read any immediately-available body
                                    clients.back().chargeBody();
                                  /* // If chargeHeader/chargeBody started a CGI, register its fds now
                                    if (clients.back().isCgiRunning()) {
                                        int inFd = clients.back().getCgiInFd();
                                        int outFd = clients.back().getCgiOutFd();
                                        if (inFd >= 0 && cgiFdToClientIdx.count(inFd) == 0)
                                            CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, inFd, clients.size() - 1, POLLOUT);
                                        if (outFd >= 0 && cgiFdToClientIdx.count(outFd) == 0)
                                            CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, outFd, clients.size() - 1, POLLIN);
                                    }*/
                                    // If header is ready and either the method isn't POST or the body is ready, send now
                                    if (clients.back().getIsHeaderReady() == true
                                        && (clients.back().getMethod() != "POST" || clients.back().getIsBodyReady() == true))
                                    {
                                        clients.back().sendResponse();
                                        if (clients.back().getIsSent() == true)
                                        {
                                            close(pollfds[pollfds.size() - 1].fd);
                                            std::swap(clients.back(), clients[clients.size() - 1]);
                                            clients.pop_back();
                                            std::swap(pollfds[pollfds.size() - 1], pollfds.back());
                                            pollfds.pop_back();
                                        }
                                        else
                                        {
                                            // If the client is waiting for a CGI to finish, do not set POLLOUT
                                            // (would cause sendResponse to be called repeatedly with no progress).
                                            if (clients.back().isCgiRunning())
                                                pollfds[pollfds.size() - 1].events = POLLIN;
                                            else
                                                pollfds[pollfds.size() - 1].events = POLLOUT;
                                        }
                                    }
                                    std::cout << "cualquier tonteria AQUI2\n";
									break;
								}
							}
                        }
                    }
                    else
					{
						// petición entrante en un cliente existente: leer/parsear
						//manageRequest(pollfds[i].fd);
						//bool handled = false;
						for (size_t j = 0; j < clients.size(); ++j)
						{
                            //std::cout << "CACA cualquier tonteria AQUI\n";
							//Imlementar en Client:
							// - handleRead(): recv() hasta terminar header/body o EGAIN
							// - prepareResponse(): preparar datos a enviar
							if (clients[j].getClientFd() == fd)
                            {
                                if (clients[j].getIsHeaderReady() == false)
                                {
                                    clients[j].chargeHeader();
                                    // If chargeHeader started a CGI, register fds immediately
                                    if (clients[j].isCgiRunning()) {
                                        int inFd = clients[j].getCgiInFd();
                                        int outFd = clients[j].getCgiOutFd();
                                        if (inFd >= 0 && cgiFdToClientIdx.count(inFd) == 0)
                                            CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, inFd, j, POLLOUT);
                                        if (outFd >= 0 && cgiFdToClientIdx.count(outFd) == 0)
                                            CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, outFd, j, POLLIN);
                                    }
                                    // If chargeHeader completed and body is ready (e.g., GET), switch to POLLOUT so we can send response
                                    if (clients[j].getIsBodyReady() == true)
                                    {
                                        if (clients[j].isCgiRunning())
                                            pollfds[i].events = POLLIN;
                                        else
                                            pollfds[i].events = POLLOUT;
                                    }
                                }
                                else
                                {
                                    clients[j].chargeBody();
                                    // If chargeBody started a CGI, register fds immediately
                                    if (clients[j].isCgiRunning()) {
                                        int inFd = clients[j].getCgiInFd();
                                        int outFd = clients[j].getCgiOutFd();
                                        if (inFd >= 0 && cgiFdToClientIdx.count(inFd) == 0)
                                            CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, inFd, j, POLLOUT);
                                        if (outFd >= 0 && cgiFdToClientIdx.count(outFd) == 0)
                                            CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, outFd, j, POLLIN);
                                    }
                                    if (clients[j].getIsBodyReady() == true)
                                    {
                                        if (clients[j].isCgiRunning())
                                            pollfds[i].events = POLLIN;
                                        else
                                            pollfds[i].events = POLLOUT;
                                    }
                                }
								break ;
                            }
						}

                        // If not a client socket, check if it's a CGI fd and dispatch
                        if (cgiFdToClientIdx.count(fd)) {
                            size_t clientIdx = cgiFdToClientIdx[fd];
                            int revents = pollfds[i].revents;
                            clients[clientIdx].handleCgiFdEvent(fd, revents);
                            // If CGI finished, unregister its fds and trigger sending response
                            if (!clients[clientIdx].isCgiRunning()) {
                                int infd = clients[clientIdx].getCgiInFd();
                                int outfd = clients[clientIdx].getCgiOutFd();
                                CgiHelpers::unregisterCgiFds(pollfds, cgiFdToClientIdx, infd, outfd);
                               // Try to send the response now that CGI is done.
                                clients[clientIdx].sendResponse();
                                // Find the pollfd for the client socket and set it to POLLOUT so sendResponse is called by the loop
                                int clientFd = clients[clientIdx].getClientFd();
                                for (size_t k = 0; k < pollfds.size(); ++k) {
                                    if (pollfds[k].fd == clientFd) {
                                        pollfds[k].events = POLLOUT;
                                        break;
                                    }
                                }
                            }
                        }
					}
				}
				if (pollfds[i].revents & POLLOUT)
				{
					for (size_t j = 0; j < clients.size(); ++j)
					{
						if (clients[j].getClientFd() == pollfds[i].fd)
						{
							// Implementar en Client:
                            // - handleWrite(): write() hasta terminar o EAGAIN
                            // - isFinished(): true si respuesta enviada y cerrar según keep-alive*/
                            std::cout << "hace el pollout\n";
                            clients[j].sendResponse();
                            if (clients[j].getIsSent() == true)
                            {
                                close(pollfds[i].fd);
                                std::swap(clients[j], clients.back());
                                clients.pop_back();
                                std::swap(pollfds[i], pollfds.back());
                                pollfds.pop_back();
                                if (i > 0)
                                    --i;
                            }
                            else
                            {
                                // If the client is waiting for CGI output, stop busy POLLOUT and wait for CGI fds.
                                if (clients[j].isCgiRunning())
                                {
                                    pollfds[i].events = POLLIN;
                                }
                            }
							/*clients[j].handlewrite();
							if (clients[j].isFinished())
							{
								clients[j].closeClient();
								close(pollfds[i].fd);
								std::swap(clients[j], clients.back());
								clients.pop_back();
								std::swap(pollfds[i], pollfds.back());
								pollfds.pop_back();
								if (i > 0)
									--i;
							}
							else
							{
								// si sigue con keep-alive, volver a escuchar lecturas
                                pollfds[i].events &= ~POLLOUT;
                                pollfds[i].events |= POLLIN;
							}
							break;*/
						}
					}
				}
            }
        }
    }
    catch (const std::exception& e)
    {
        //std::cout << e.what() << std::endl;
		std::cerr << "Uncaught exception (" << typeid(e).name() << "): " << e.what() << std::endl;
        return 1;
    }
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