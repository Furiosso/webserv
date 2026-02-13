#ifndef SERVERSOCKET_HPP
# define SERVERSOCKET_HPP

# include <sys/socket.h>
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
# include "utils.hpp"

class ServerSocket
{
    private:
        int                 _fd;
        //std::string         _host;
        //const char*         _port;
        //struct sockaddr_in  _addr; //bind()
        //struct addrinfo     _addr; // crear sockets |||||| bind(sockfd, res->aiddr, res->ai_addrlen)
        
    public:
        ServerSocket(const char* port);
        ~ServerSocket();
        int get_fd();
};

#endif