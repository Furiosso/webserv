#include "ServerSocket.hpp"
#include <poll.h>
#include <map>
#include <vector>
#include <string>
#include "Parser.hpp"
#include "Server.hpp"
#include "Client.hpp"
#include <typeinfo>
#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>


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
    if (argc != 2)
    {
        std::cerr << "Invalid arguments\n";
        return 1;
    }
    try
    {
        std::vector<Server> servers;
        Parser              parser(argv[1], servers);
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
        // Setup signalfd to catch termination signals inside the poll loop
        int sigfd = -1;
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTERM);
        sigaddset(&mask, SIGHUP);
        // Block these signals so they are delivered via signalfd
        if (sigprocmask(SIG_BLOCK, &mask, NULL) == 0) {
            sigfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
            if (sigfd >= 0) {
                struct pollfd p; p.fd = sigfd; p.events = POLLIN; p.revents = 0; pollfds.push_back(p);
            } else {
                std::cerr << "Warning: failed to create signalfd: " << strerror(errno) << "\n";
            }
        } else {
            std::cerr << "Warning: failed to block signals for signalfd: " << strerror(errno) << "\n";
        }
        /*Una línea, pero hay un problema importante aquí relacionado con el punto 3 y 4 que ya mencioné:
        Confirma el bug de double-close y código muerto
        Con esta línea copiás el vector de pollfds de sockman al pollfds local del main. Eso significa:

        ServerSocket::_pollfds sí se usa (se copia aquí) — así que el punto 4 queda descartado, bien.
        Pero ahora tienes los mismos fds en dos sitios: sockman._listeners y pollfds del main. Cuando el main cierra fds individualmente con close(pollfds[i].fd) a lo largo del loop, y luego el destructor de sockman llama a closeAll() al salir del scope, esos mismos fds se cierran dos veces. Eso es UB y puede causar cierre accidental de fds reabiertos por el OS con el mismo número.

        La solución más limpia para webserv:
        Añade un método ServerSocket::clearListeners() que vacíe _listeners sin cerrar nada, y llámalo justo después de esta línea:
        std::vector<struct pollfd> pollfds = sockman.getPollfds();
        sockman.clearListeners(); // transfiere "ownership" al main
        Así el destructor de sockman no cierra nada, y el main es el único dueño de los fds.
        Pasa el siguiente fragmento.*/
        std::vector<Client> clients;
        // Map CGI fds (in_fd/out_fd) to client index in `clients` vector
    // Map CGI fd -> client socket fd (int). We store client socket fd instead
    // of vector index to avoid stale indexes when clients vector is mutated.
    std::map<int, int> cgiFdToClientIdx;

    // helper functions (C++98 compatible)
            struct CgiHelpers {
                static void registerCgiFd(std::vector<struct pollfd>& pollfds, std::map<int, int>& map, int fd, int clientSock, short events) {
                    if (fd < 0) return;
                    struct pollfd p; p.fd = fd; p.events = events; p.revents = 0; pollfds.push_back(p);
                    map[fd] = clientSock;
                    std::cerr << "registerCgiFd: registered fd=" << fd << " for clientSock=" << clientSock << " events=" << events << "\n";
                }
                static void unregisterCgiFds(std::vector<struct pollfd>& pollfds, std::map<int, int>& map, int infd, int outfd) {
                    if (infd >= 0) map.erase(infd);
                    if (outfd >= 0) map.erase(outfd);
                    for (size_t k = 0; k < pollfds.size(); ) {
                        if (pollfds[k].fd == infd || pollfds[k].fd == outfd) {
                            std::cerr << "unregisterCgiFds: removing fd=" << pollfds[k].fd << "\n";
                            /*if (pollfds[k].fd > 0)
								close(pollfds[k].fd);*/
                            std::swap(pollfds[k], pollfds.back());
                            pollfds.pop_back();
                        } else ++k;
                    }
                }
                // Update map entries when swapping/removing clients:
                // cgiFdToClientIdx maps CGI fd -> client socket fd (int), so removal/reassignment operate on client socket fds.
                static void removeClientMappings(std::map<int, int>& map, int clientSock) {
                    for (std::map<int,int>::iterator it = map.begin(); it != map.end(); ) {
                        if (it->second == clientSock) {
                            std::map<int,int>::iterator toErase = it;
                            ++it;
                            map.erase(toErase);
                        } else ++it;
                    }
                }
                // No-op for reassign when mapping by client socket fd; kept for API compatibility.
                static void reassignMappingsOnSwap(std::map<int, int>& /*map*/, size_t /*idxRemoved*/, size_t /*idxMovedTo*/) {
                    // Intentionally empty: cgiFdToClientIdx uses client socket fds, so vector-index-based reassignment is not required.
                }
            };
    bool stop = false;
    while (!stop)
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
                    CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, inFd, clients[ci].getClientFd(), POLLOUT);
                }
                if (outFd >= 0 && cgiFdToClientIdx.count(outFd) == 0) {
                    CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, outFd, clients[ci].getClientFd(), POLLIN);
                }
            }

            for(std::vector<struct pollfd>::size_type i = 0; i < pollfds.size(); ++i)
            {
                // Debug: show which fd has revents set
                
                // check for signal fd first
                if (sigfd >= 0 && pollfds[i].fd == sigfd && (pollfds[i].revents & POLLIN)) {
                    struct signalfd_siginfo fdsi;
                    ssize_t s = read(sigfd, &fdsi, sizeof(fdsi));
                    if (s == sizeof(fdsi)) {
                        if (fdsi.ssi_signo == SIGINT || fdsi.ssi_signo == SIGTERM) {
                            std::cerr << "Received signal to terminate (" << fdsi.ssi_signo << "), shutting down...\n";
                            stop = true;
                            break; // break out of pollfds for-loop
                        } else if (fdsi.ssi_signo == SIGHUP) {
                            std::cerr << "Received SIGHUP (" << fdsi.ssi_signo << ") - ignoring for now\n";
                        }
                    }
                    continue;
                }
                //comprobar signals
                if (pollfds[i].revents & POLLHUP)
				{
				    int fd = pollfds[i].fd;
				    if (cgiFdToClientIdx.count(fd)) {
				        // Es un pipe CGI — dejar que handleCgiFdEvent lo gestione
				        int clientSock = cgiFdToClientIdx[fd];
				        for (size_t ci = 0; ci < clients.size(); ++ci) {
				            if (clients[ci].getClientFd() == clientSock) {
				                clients[ci].handleCgiFdEvent(fd, pollfds[i].revents);
				                if (!clients[ci].isCgiRunning()) {
				                    int infd = clients[ci].getCgiInFd();
				                    int outfd = clients[ci].getCgiOutFd();
				                    CgiHelpers::unregisterCgiFds(pollfds, cgiFdToClientIdx, infd, outfd);
				                }
				                break;
				            }
				        }
				        // No cerrar el pollfd aquí, unregisterCgiFds ya lo eliminó si procede
				        continue;
				    }
				    // fd de cliente normal
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
                                        // If sendResponse started a CGI, register its fds immediately
                                        if (clients.back().isCgiRunning()) {
                                            int inFd = clients.back().getCgiInFd();
                                            int outFd = clients.back().getCgiOutFd();
                                            if (inFd >= 0 && cgiFdToClientIdx.count(inFd) == 0)
                                                CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, inFd, clients.back().getClientFd(), POLLOUT);
                                            if (outFd >= 0 && cgiFdToClientIdx.count(outFd) == 0)
                                                CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, outFd, clients.back().getClientFd(), POLLIN);
                                        }
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
                                                CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, inFd, clients[j].getClientFd(), POLLOUT);
                                            if (outFd >= 0 && cgiFdToClientIdx.count(outFd) == 0)
                                                CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, outFd, clients[j].getClientFd(), POLLIN);
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
									std::cerr << "main cgi if chargebody\n";
                                    if (clients[j].isCgiRunning()) {
                                        int inFd = clients[j].getCgiInFd();
                                        int outFd = clients[j].getCgiOutFd();
                                        if (inFd >= 0 && cgiFdToClientIdx.count(inFd) == 0)
                                            CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, inFd, clients[j].getClientFd(), POLLOUT);
                                        if (outFd >= 0 && cgiFdToClientIdx.count(outFd) == 0)
                                            CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, outFd, clients[j].getClientFd(), POLLIN);
                                    }
                                    if (clients[j].getIsBodyReady() == true)
                                    {
                                        /*if (clients[j].isCgiRunning())
                                            pollfds[i].events = POLLIN;
                                        else*/
										clients[j].sendResponse();
										pollfds[i].events = POLLOUT;
                                    }
                                }
								break ;
                            }
						}

                        // If not a client socket, check if it's a CGI fd and dispatch
                        if (cgiFdToClientIdx.count(fd)) {
                            int clientSock = cgiFdToClientIdx[fd];
                            size_t clientIdx = static_cast<size_t>(-1);
                            for (size_t ci = 0; ci < clients.size(); ++ci) {
                                if (clients[ci].getClientFd() == clientSock) { clientIdx = ci; break; }
                            }
                            if (clientIdx == static_cast<size_t>(-1)) {
                                // client not found (might have been closed) -> unregister fds
                                CgiHelpers::unregisterCgiFds(pollfds, cgiFdToClientIdx, fd, -1);
                            } else {
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
                            // If sendResponse started a CGI, register its fds immediately
                            if (clients[j].isCgiRunning()) {
                                int inFd = clients[j].getCgiInFd();
                                int outFd = clients[j].getCgiOutFd();
                                if (inFd >= 0 && cgiFdToClientIdx.count(inFd) == 0)
                                    CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, inFd, clients[j].getClientFd(), POLLOUT);
                                if (outFd >= 0 && cgiFdToClientIdx.count(outFd) == 0)
                                    CgiHelpers::registerCgiFd(pollfds, cgiFdToClientIdx, outFd, clients[j].getClientFd(), POLLIN);
                            }
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

