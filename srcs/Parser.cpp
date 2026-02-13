#include "Parser.hpp"

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
            this->_config_file = this->_config_file + cleanLine + " ";
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
    for (size_t i = 0; i < this->_tokens.size(); i++)
       std::cout << this->_tokens[i] << std::endl;
       //std::cout << this->_config_file[i] << std::endl;
       
    config_file.close();
}

Parser::~Parser()
{}

void    Parser::tokenize()
{
    std::string token;
    std::string line;
    for (size_t i = 0; this->_config_file[i];++i)
    {
        switch(this->_config_file[i])
        {
            case '{':
                _tokens.push_back("{");
                break ;
            case '}':
                _tokens.push_back(token);
                token.clear();
                _tokens.push_back("}");
                break;
            case ';':
                _tokens.push_back(token);
                token.clear();
                _tokens.push_back(";");
            case ' ':
                _tokens.push_back(token);
                token.clear();
            default:
                token.push_back(this->_config_file[i]);
        }
    }
}
