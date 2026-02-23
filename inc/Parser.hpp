#ifndef PARSER_HPP
# define PARSER_HPP

# include <string>
# include <vector>
# include <fstream>
# include <iostream>
# include <map>
# include <sstream>
# include <iterator>
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
    "allowed_methods"
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

enum IPV4State
{
    IP_ER,
    IP_00,
    IP_01,
    IP_02,
    IP_03,
    IP_05,
    IP_PO,
    IP_NU
};

class Parser
{
    private:
        std::string                         _config_file;
        std::vector<std::string>            _tokens;
        std::map<std::string, std::string>  _listens;
        std::vector<int>                    _lflags;
        void                                listenParser(std::vector<std::string>::iterator& it);
        bool                                check_ipv4(std::string t);
        bool                                check_port(std::string t);
        int                                 chooseState(std::vector<std::string>& tokens);
        int                                 getIPV4State(int prev, int pos);
    public:
        Parser(const char* in_file);
        //Parser(const Parser& other);
        ~Parser();
        //Parser& operator=(const Parser& other);
        void    tokenize();
        void                        rmComments(std::ifstream& config_file);
};

#endif