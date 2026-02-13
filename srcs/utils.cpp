#include "utils.hpp"


void    ft_bzero(void *s, size_t n)
{
    size_t          i;
    unsigned char*  buff; 

    buff = static_cast<unsigned char*>(s);
    i = 0;
    while (i < n && buff[i])
    {
        buff[i] = 0;
        ++i;
    }
}

std::string rtrim(std::string& line)
{
    size_t end = line.size() - 1;

    if (line.empty())
        return line;
    while ((end > 0 && std::isspace(static_cast<unsigned char>(line[end]))))
        end--;
    if (end == 0 && std::isspace(static_cast<unsigned char>(line[0])))
        return "";
    return line.substr(0, end + 1);
}

std::string trim(std::string& line)
{
    size_t i = 0;

    if (line.empty())
        return line;
    while (line[i] && std::isspace(static_cast<unsigned char>(line[i])))
        i++;
    if (i == line.size())
        return "";
    return line.substr(0, line.size() - i);
}