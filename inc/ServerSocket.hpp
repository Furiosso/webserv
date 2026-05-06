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
        std::vector<int>                                _listeners;
        std::vector<struct pollfd>                      _pollfds;
        std::set< std::pair<std::string, std::string> > _created;
        
    public:
    /**
     * Constructor por defecto de ServerSocket.
     * Inicializa las estructuras internas.
     */
    ServerSocket();

    /**
     * Destructor de ServerSocket.
     * Cierra listeners y limpia recursos asociados.
     */
    ~ServerSocket();

    /**
     * Devuelve la lista de descriptores de escucha creados.
     *
     * @return Referencia constante al vector de descriptors (listeners).
     */
    const std::vector<int>&             getListeners() const;

    /**
     * Comprueba si un descriptor corresponde a un listener conocido.
     *
     * @param[in] fd Descriptor a comprobar.
     * @return true si el descriptor es uno de los listeners.
     */
    bool                                isListener(int fd);

    /**
     * Crea listeners (sockets) para los `Server` pasados en configuración.
     *
     * @param[in,out] servers Vector de servidores parseados con sus configuraciones.
     * @return true si al menos un listener fue creado correctamente.
     */
    bool                                createListeners(std::vector<Server>& servers);

    /**
     * Acepta una nueva conexión entrante en el listener especificado.
     *
     * @param[in] listen_fd Descriptor del socket listener donde aceptar.
     * @return Descriptor del nuevo cliente o -1 en caso de error.
     */
    int                                 acceptNewClient(int listen_fd);

    /**
     * Devuelve el vector de `pollfd` usado para multiplexado (poll).
     *
     * @return Referencia constante al vector de `pollfd`.
     */
    const std::vector<struct pollfd>&   getPollfds() const;

    /**
     * Cierra todos los listeners y reinicia las estructuras internas.
     */
    void                                closeAll();
};

#endif