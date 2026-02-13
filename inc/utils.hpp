#ifndef UTILS_HPP
# define UTILS_HPP

# include <stddef.h>
# include <iostream>
# include <string>


void		ft_bzero(void* s, size_t n);
std::string rtrim(std::string& line);
std::string trim(std::string& line);

#endif