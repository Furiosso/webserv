#ifndef PARSER_HPP
# define PARSER_HPP

# include <string>
# include <vector>
# include <fstream>
# include <iostream>
# include <sstream>
# include "utils.hpp"

const std::string   configkeys[15] =
{
    "listen",
    "server_name",
    "error_page",
    "client_max_body_size",
    "root",
    "index",
    "autoindex",
    "cgi_path",
    "cgi_ext",
    "return",
    "upload_store",
    "upload_pass",
    "cgi_pass",
    "alias",
    "accept_method"
};

typedef enum s_token
{
    A_INI, //inicial
    A_ERR, //error
    A_SER, //server
    A_BFI, //llave de apertura
    A_LIS, //listen
    A_ROO, //root
    A_SEM, //punto y coma;
    A_BLA, //llave de cesura
    A_INP, //input
    A_LOC,  //location
    A_LIN  //Location input
} t_token;

class Parser
{
    private:
        std::string                 _config_file;
        std::vector<std::string>    _tokens;
    public:
        Parser(const char* in_file);
        //Parser(const Parser& other);
        ~Parser();
        //Parser& operator=(const Parser& other);
        void    tokenize();
        void                        rmComments(std::ifstream& config_file);
};

#endif