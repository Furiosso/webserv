#ifndef SERVER_HPP
# define SERVER_HPP

# include <map>
# include <string>
# include <vector>

struct LocationConfig //durante la elaboración del ejercicio nos hemos dado cuenta de que Location debería ser una clase hija de Server. Ya que funciona lo dejamos así, pero no es lo ideal
{
	std::string									path;
	std::string									root;
	std::vector<std::string>					index;
	std::vector<std::string>					allowed_methods;
    std::map<std::string, std::string>			cgi;
	std::map<std::pair<int, int>, std::string>	error_pages;
	bool										areErrorPages;
	size_t										client_max_body_size;
	bool										autoindex;
	bool										isAutoindex;
	bool										isRoot;
	bool										isAlias;
};


struct ServerConfig
{
    std::multimap<std::string, std::string>		listen;
    std::map<std::string, std::string>			cgi;
    std::vector<std::string>					index;
	bool										autoindex;
	std::map<std::pair<int, int>, std::string>	error_pages;
	bool										areErrorPages;
	std::string									root;
	std::string									server_name;
	size_t										client_max_body_size;
	std::vector<LocationConfig>					locations;
	std::vector<std::string>					allowed_methods;
	bool										isAutoindex;
	bool										isRoot;
	bool										isAlias;
};


class Server
{
private:
    ServerConfig	_config;
	int				_fd;
public:
	/**
	 * Constructor por defecto de Server.

	 */
	Server();

	/**
	 * Destructor de Server.

	 */
	~Server();

	/**
	 * Añade una IP:PUERTO a la lista de escucha del servidor.

	 * @param[in] ip Dirección IP como cadena.
	 * @param[in] port Puerto como cadena.

	 */
	void	addListen(std::string& ip, std::string& port);

	/**
	 * Registra un mapeo extensión -> ejecutable CGI.

	 * @param[in] ext Extensión (ej: ".py").
	 * @param[in] path Ruta al ejecutable o intérprete.

	 */
	void	addCgi(std::string& ext, std::string &path);

	/**
	 * Añade un nombre de fichero a la lista de índices.

	 * @param[in] name Nombre del fichero índice.

	 */
	void	addIndex(std::string& name);

	/**
	 * Activa o desactiva el autoindex para este servidor.

	 * @param[in] aI true para activar, false para desactivar.

	 */
	void	setAutoindex(bool aI);

	/**
	 * Establece la raíz (o alias) del servidor.

	 * @param[in] r Ruta a establecer.
	 * @param[in] n 0 si es root, 1 si es alias.

	 */
	void	setRoot(std::string& r, int n);

	/**
	 * Establece el nombre del servidor (server_name).

	 * @param[in] sn Nombre del servidor.

	 */
	void	setServerName(std::string& sn);

	/**
	 * Ajusta el límite máximo de tamaño de cuerpo para clientes.

	 * @param[in] cmbs Número base.
	 * @param[in] c Unidad (K, M, G, ...).

	 */
	void	setClientMaxBodySize(long long cmbs, char c);

	/**
	 * Añade una página de error para rangos de códigos.

	 * @param[in] codes Vector de pares que indican código/clase.
	 * @param[in] uri URI de la página de error.

	 */
	void	addErrorPage(std::vector <std::pair<int, int> > codes, std::string& uri);

	/**
	 * Añade una configuración de location al servidor.

	 * @param[in] loc Configuración de location (por referencia).

	 */
	void	addLocation(LocationConfig& loc);

	/**
	 * Establece la lista de métodos permitidos para este servidor.

	 * @param[in] m Vector con los métodos permitidos.

	 */
	void	setAllowedMethods(const std::vector<std::string>& m);

	/**
	 * Asigna el descriptor de escucha asociado a este servidor.

	 * @param[in] fd Descriptor de fichero del socket.

	 */
	void	setFd(int fd);

	/**
	 * Devuelve el descriptor de escucha del servidor.

	 * @return Descriptor de fichero o -1 si no establecido.

	 */
	int	getFd() const;

	/**
	 * Devuelve la configuración interna del servidor (referencia constante).

	 * @return Referencia constante a `ServerConfig`.

	 */
	const ServerConfig&	getConfig() const;
};

#endif