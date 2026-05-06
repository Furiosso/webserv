#ifndef UTILS_HPP
# define UTILS_HPP

# include <stddef.h>
# include <iostream>
# include <string>
# include <sstream>
# include <cctype>

/**
 * Rellena con ceros `n` bytes a partir del puntero `s`.
 *
 * @param[in] s Puntero al buffer a inicializar.
 * @param[in] n Número de bytes a poner a cero.
 */
void		ft_bzero(void* s, size_t n);

/**
 * Elimina los espacios en blanco al final de una cadena.
 *
 * @param[in] line Cadena a procesar (se pasa por referencia).
 * @return Nueva cadena con los espacios finales eliminados.
 */
std::string rtrim(std::string& line);

/**
 * Elimina espacios en blanco al principio y al final de una cadena.
 *
 * @param[in] line Cadena a procesar (se pasa por referencia).
 * @return Nueva cadena recortada.
 */
std::string trim(std::string& line);

/**
 * Comprueba si una cadena contiene solo dígitos decimales.
 *
 * @param[in] str Cadena a comprobar (se pasa por valor).
 * @return true si todos los caracteres son dígitos, false en caso contrario.
 */
bool        strIsDigit(std::string str);

/**
 * Cuenta las palabras en una cadena separadas por un carácter separador.
 *
 * @param[in] line Cadena a analizar (se pasa por referencia).
 * @param[in] splitter Carácter separador.
 * @return Número de palabras encontradas.
 */
size_t		wordCounter(std::string& line, char splitter);

/**
 * Devuelve una copia de la cadena en minúsculas.
 *
 * @param[in] s Cadena a convertir (se pasa por referencia).
 * @return Nueva cadena en minúsculas.
 */
std::string strToLower(std::string& s);

/**
 * Comprueba si un nombre de fichero termina con la extensión indicada.
 *
 * @param[in] name Nombre de fichero o ruta (se pasa por referencia).
 * @param[in] extention Extensión a comprobar (por ejemplo: ".txt").
 * @return true si `name` termina con `extention`, false en caso contrario.
 */
bool        checkExtention(std::string& name, std::string extention);

/**
 * Convierte una cadena hexadecimal a su valor decimal.
 *
 * @param[in] hexStr Cadena con dígitos hexadecimales (sin prefijo 0x).
 * @return Valor decimal representado por la cadena.
 */
size_t      hexToDecimal(const std::string &hexStr);

/**
 * Extrae la extensión (incluyendo el punto) de una ruta o nombre de fichero.
 *
 * @param[in] path Ruta o nombre de fichero.
 * @return Extensión encontrada (ej. ".html") o cadena vacía si no existe.
 */
std::string getExtension(const std::string& path);

#endif