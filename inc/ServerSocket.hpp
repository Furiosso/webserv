#ifndef SERVERSOCKET_HPP
# define SERVERSOCKET_HPP

# include <sys/socket.h>
# include <sys/poll.h>
# include <netinet/in.h>
# include <sys/types.h>
# include <netdb.h>
# include <cstdio>
# include <unistd.h>
# include <string>
# include <errno.h>
# include <string.h>
# include <iostream>
# include <fcntl.h>
# include <fstream>
# include <arpa/inet.h>
# include <set>
# include "utils.hpp"
# include "Server.hpp"

class ServerSocket
{
    private:
        //int                 _fd;
        std::vector<int>                                _listeners;
        std::vector<struct pollfd>                      _pollfds;
        /*
        4. _pollfds en ServerSocket no se usa
        En el main construís tu propio vector pollfds — el de ServerSocket se rellena pero nunca se consulta. Es código muerto. O lo usás o lo eliminás.
        */
        std::set< std::pair<std::string, std::string> > _created; //ver utilidad (comprobacion de duplicados ip:port)
        //std::string         _host;
        //const char*         _port;
        //struct sockaddr_in  _addr; //bind()
        //struct addrinfo     _addr; // crear sockets |||||| bind(sockfd, res->aiddr, res->ai_addrlen)
        
    public:
        ServerSocket();
       // ServerSocket(const char* port);
        ~ServerSocket();
        const std::vector<int>&             getListeners() const;
        bool                                isListener(int fd);
        bool                                createListeners(std::vector<Server>& servers);
        int                                 acceptNewClient(int listen_fd);
        const std::vector<struct pollfd>&   getPollfds() const;
        void                                closeAll();
};

#endif