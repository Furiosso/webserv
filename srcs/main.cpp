#include "ServerSocket.hpp"
#include <poll.h>
#include <map>

int main (int argc, char** argv, char** env)
{
    (void)argv;
    (void)env;
    if (argc != 2)
    {
        // include error message here
        return 1;
    }
    ServerSocket prueba("80");
    struct pollfd* fds;
    fds->fd = prueba.get_fd();
    fds->events = POLLIN;
    int nfds = 1;
    while (true)
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
    }
}