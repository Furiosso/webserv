#include "Parser.hpp"

int     getState(int prev, int pos)
{
    static int tokens[][11] = {
        {S_ERR, S_SER, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR}, //  0 INI
        {S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR}, //  1 Error
        {S_ERR, S_ERR, S_SOP, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR}, //  2 SERVER
        {S_ERR, S_ERR, S_ERR, S_ERR, S_LOC, S_ERR, S_ERR, S_ERR, S_KEY, S_ERR, S_ERR}, //  3 SERVER_OP
        {S_ERR, S_SER, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR}, //  4 SERVER_CL
        {S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_LUR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR}, //  5 LOCATION
        {S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_LOP, S_ERR, S_ERR, S_ERR, S_ERR}, //  6 LOCATION_URI
        {S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_LCL, S_KEY, S_ERR, S_ERR}, //  7 LOCATION_OP
        {S_ERR, S_ERR, S_ERR, S_SCL, S_LOC, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR}, //  8 LOCATION_CL
        {S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_PAR, S_ERR}, //  9 KEYWORD
        {S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_ERR, S_PAR, S_SEM}, // 10 PARAMETER
        {S_ERR, S_ERR, S_ERR, S_SCL, S_LOC, S_ERR, S_ERR, S_LCL, S_KEY, S_ERR, S_ERR}, // 11 SEMICOLON
    };
    return tokens[prev][pos];
}

int    isConfigWord(std::string& token)
{
    for (size_t i = 0; i < 15; i++)
    {
        if (token == configkeys[i])
            return 1;
    }
    return 0;
}

int     Parser::chooseState(std::vector<std::string>& tokens)
{
    int prev = 0;
    int  is_serv;
    int  is_loc;
    for (size_t i = 0; i < tokens.size(); i++)
    {
        int pos = 0;
        if (tokens[i] == "server")
            pos = 1;
        else if (tokens[i] == "{" && prev == 2)
        {
            pos = 2;
            is_serv = 1;
        }
        else if (tokens[i] == "{" && prev == 6)
        {
            pos = 6;
            is_loc = 1;
        }
        else if (tokens[i] == "}")
        {
            if ((prev == 3 || prev == 8 || prev == 11) && is_serv)
            {
                pos = 3;
                if (!is_loc)
                    is_serv = 0;
            }
            if ((prev == 7 || prev == 11) && is_loc)
            {
                pos = 7;
                is_loc = 0;
            }
        }
        else if (tokens[i] == "location")
            pos = 4;
        else if (isConfigWord(tokens[i]))
            pos = 8;
        else if (tokens[i] == ";")
            pos = 10;
        else
        {
            pos = 9;
            if (prev == 5)
                pos = 5;
        }
        //std::cout << "prev:" << prev << " pos: " << pos << " tokens[i]: " << tokens[i] << "\n";
        prev = getState(prev, pos);
        if (prev == 1)
        {
            std::cerr << "Syntax error\n";
            return 1;
        }
    }
    return 0;
}

void    Parser::rmComments(std::ifstream& config_file)
{
    std::string line;

    while (std::getline(config_file, line))
    {
        if (line.empty())
            this->_config_file = this->_config_file + " ";
        if (!line.empty())
        {
            size_t  pos = line.find('#');
            if (pos != std::string::npos)
                line = line.substr(0, pos);
            std::string cleanLine = rtrim(line); //desarrollar eliminar espacios
            if (!cleanLine.empty())
                this->_config_file += cleanLine + " ";
        }
    }
}

/*void    Parser::listenParser(std::vector<std::string>& tokens)
{

}*/

Parser::Parser(const char* in_file)
{
	std::ifstream   config_file(in_file);
    if (!config_file.is_open())
        throw std::runtime_error("Could not open config file\n");
    //tokenizar y parsear
    this->rmComments(config_file);
    this->tokenize();
    if (this->chooseState(this->_tokens))
        ;
    //this->listenParser();
    /*for (size_t i = 0; i < this->_tokens.size(); i++)
       std::cout << this->_tokens[i] << std::endl;
    for (size_t i = 0; i < this->_config_file.size(); i++)
       std::cout << this->_config_file[i];*/
       
    config_file.close();
}

Parser::~Parser()
{}

void    Parser::tokenize()
{
    std::string token;
    int         curly_braces = 0;

    for (size_t i = 0; i < this->_config_file.size();++i)
    {
        switch(this->_config_file[i])
        {
            case '{':
                if (!token.empty())
                {
                    _tokens.push_back(token);
                    token.clear();
                }
                _tokens.push_back("{");
                ++curly_braces;
                //if (curly_braces > 2)
                    // cerrar  config_file y devolver error o excepcion
                break ;
            case '}':
                if (!token.empty())
                {
                    _tokens.push_back(token);
                    token.clear();
                }
                _tokens.push_back("}");
                --curly_braces;
                break;
            case ';':
                if (!token.empty())
                {
                    _tokens.push_back(token);
                    token.clear();
                }
                _tokens.push_back(";");
                break ;
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                if (!token.empty())
                {
                    _tokens.push_back(token);
                    token.clear();
                }
                break;
            default:
                token.push_back(this->_config_file[i]);
                break;
        }
    }
    if (!token.empty())
        _tokens.push_back(token);
    if (curly_braces != 0)
    {
        //cerrar el config_file y devolver error;
    }
}
