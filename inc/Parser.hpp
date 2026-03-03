#ifndef PARSER_HPP
# define PARSER_HPP

# include <string>
# include <vector>
# include <fstream>
# include <iostream>
# include <map>
# include <sstream>
# include <iterator>
# include <limits>
# include <unistd.h>
# include "utils.hpp"
# include "Server.hpp"

const std::string   configkeys[16] =
{
    "listen", //done
    "server_name", // done
    "error_page", // done
    "client_max_body_size", // done
    "root", // done
    "location",
    "index", //done
    "autoindex", // done
    "return",
    "upload_store",
    "upload_pass",
    "cgi", // done
    "alias", // to be done
    "allowed_methods" // done
};

const std::string   cgikeys[3] =
{
    ".py",
    ".php",
    ".pl" 
};

enum    State
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

enum    IPV4State
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

enum    bodySizeState
{
    BD_ERR,
    BD_NUM,
    BD_CHR
};

enum    errorpageState
{
    EP_ERR,
    EP_COD,
    EP_OVR,
    EP_URI
};

enum    locationState
{
    LO_ERR,
    LO_EQU,
    LO_PAT
};

class Parser
{
    private:
        std::string                         _config_file;
        std::vector<std::string>            _tokens;
        std::map<std::string, std::string>  _listens;
        std::vector<int>                    _lflags;
        int                                 _serverCounter;
        void                                listenParser(std::vector<std::string>::iterator& it, Server& server); //not
        void                                autoindexParser(std::vector<std::string>::iterator& it, Server& server); //not
        void                                allowedParser(std::vector<std::string>::iterator& it, Server& server); //not
        void                                clientmaxbodysizeParser(std::vector<std::string>::iterator& it, Server& server); //no
        void                                serverNameParser(std::vector<std::string>::iterator& it, Server& server); //not
        void                                errorpageParser(std::vector<std::string>::iterator& it, Server& server); //not
        void                                cgiParser(std::vector<std::string>::iterator& it, Server& server); // done
        void                                rootParser(std::vector<std::string>::iterator& it, Server& server); // done
        void                                locationParser(std::vector<std::string>::iterator& it, Server& server); //not
        int		                            getServerNameState(int prev, int pos);
        bool                                checkclientmaxbodysize(std::string t);
        bool                                check_ipv4(std::string t);
        bool                                check_port(std::string t);
        int                                 chooseState(std::vector<std::string>& tokens);
        int                                 getIPV4State(int prev, int pos);
        int                                 getClientMaxBodySizeState(int prev, int pos);
        int                                 getErrorPageParserState(int prev, int pos);
        int                                 getTypeOfItem(std::string& str);
        void                                indexParser(std::vector<std::string>::iterator& it, Server& server);// done
        void	                            chooseIndexState(std::string str);
        int                                 getIndexState(int prev, int pos);
        int                                 getLocationState(int prev, int pos);

    public:
        Parser(const char* in_file, std::vector<Server>& servers);
        //Parser(const Parser& other);
        ~Parser();
        //Parser& operator=(const Parser& other);
        void                        tokenize();
        void                        rmComments(std::ifstream& config_file);
        std::vector<std::string>    get_tokens();
};

#endif