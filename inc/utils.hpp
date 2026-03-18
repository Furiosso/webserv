#ifndef UTILS_HPP
# define UTILS_HPP

# include <stddef.h>
# include <iostream>
# include <string>
# include <sstream>
# include <cctype>


void		ft_bzero(void* s, size_t n);
std::string rtrim(std::string& line);
std::string trim(std::string& line);
bool        strIsDigit(std::string str);
size_t		wordCounter(std::string& line, char splitter);
std::string strToLower(std::string& s);
bool        checkExtention(std::string& name, std::string extention);
size_t      hexToDecimal(const std::string &hexStr);

#endif