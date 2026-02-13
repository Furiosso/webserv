#ifndef PARSER_HPP
# define PARSER_HPP

# include <string>
# include <vector>
# include <fstream>
# include <iostream>
# include <sstream>
# include "utils.hpp"

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