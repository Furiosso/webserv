#include "Parser.hpp"

int     chooseState(int prev, int pos)
{
    static int tokens[][11] = {
        {1, 2, 1, 1, 1, 1, 1, 1, 1,  1,  1}, //  0 INI
        {1, 1, 1, 1, 1, 1, 1, 1, 1,  1,  1}, //  1 Error
        {1, 1, 3, 1, 1, 1, 1, 1, 1,  1,  1}, //  2 SERVER
        {1, 1, 1, 1, 5, 1, 1, 1, 9,  1,  1}, //  3 SERVER_OP
        {1, 2, 1, 1, 1, 1, 1, 1, 1,  1,  1}, //  4 SERVER_CL
        {1, 1, 1, 1, 1, 6, 1, 1, 1,  1,  1}, //  5 LOCATION
        {1, 1, 1, 1, 1, 1, 7, 1, 1,  1,  1}, //  6 LOCATION_URI
        {1, 1, 1, 1, 1, 1, 1, 8, 9,  1,  1}, //  7 LOCATION_OP
        {1, 1, 1, 4, 5, 1, 1, 1, 1,  1,  1}, //  8 LOCATION_CL
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 10,  1}, //  9 KEYWORD
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 10, 11}, // 10 PARAMETER
        {1, 1, 1, 1, 5, 1, 1, 8, 9,  1,  1}, // 11 SEMICOLON
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

int     getterState(std::vector<std::string>& tokens)
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
        prev = chooseState(prev, pos);
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

Parser::Parser(const char* in_file)
{
	std::ifstream   config_file(in_file);
    if (!config_file.is_open())
        throw std::runtime_error("Could not open config file\n");
    //tokenizar y parsear
    this->rmComments(config_file);
    this->tokenize();
    if (getterState(this->_tokens))
        ;
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
