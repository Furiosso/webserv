#include <stddef.h>

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