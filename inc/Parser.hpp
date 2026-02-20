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

enum State
{
    S_INI,
    S_ERR,
    S_SER,
    S_SOP,
    S_SCL,
    S_LOC,
    S_LUR,
    S_LOP,
    S_LCL,
    S_KEY,
    S_PAR,
    S_SEM
};

class Parser
{
    private:
        std::string                 _config_file;
        std::vector<std::string>    _tokens;
        //void                        listenParser(std::vector<std::string>& tokens);
        int                         chooseState(std::vector<std::string>& tokens);
    public:
        Parser(const char* in_file);
        //Parser(const Parser& other);
        ~Parser();
        //Parser& operator=(const Parser& other);
        void    tokenize();
        void                        rmComments(std::ifstream& config_file);
};

#endif