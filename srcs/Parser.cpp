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
        {S_ERR, S_ERR, S_ERR, S_SCL, S_LOC, S_ERR, S_ERR, S_ERR, S_KEY, S_ERR, S_ERR}, //  8 LOCATION_CL
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

int     Parser::getIPV4State(int prev, int pos)
{
    static int tokens[][8] = {
        {IP_ER, IP_00, IP_01, IP_02, IP_NU, IP_NU, IP_ER, IP_ER}, //  0 INI
        {IP_ER, IP_ER, IP_ER, IP_ER, IP_ER, IP_ER, IP_PO, IP_ER}, // IP_00
        {IP_ER, IP_01, IP_01, IP_01, IP_01, IP_01, IP_PO, IP_01}, // IP_01
        {IP_ER, IP_01, IP_01, IP_01, IP_01, IP_05, IP_PO, IP_00}, // IP_02
        {IP_ER, IP_ER, IP_ER, IP_ER, IP_ER, IP_ER, IP_ER, IP_ER}, // IP_03
        {IP_ER, IP_00, IP_00, IP_00, IP_00, IP_00, IP_PO, IP_ER}, // IP_05
        {IP_ER, IP_00, IP_01, IP_02, IP_NU, IP_NU, IP_ER, IP_NU}, // IP_PO
        {IP_ER, IP_00, IP_00, IP_00, IP_00, IP_00, IP_PO, IP_00} // IP_NU
        //{}, // IP_22
    };
    return tokens[prev][pos];
}

bool    Parser::check_ipv4(std::string t)
{
    int prev = 0;
    int pos;
    int len = 0;
    size_t i = 0;

    if (t == "localhost")
        return (true);
    for (; i < t.length(); i++)
    {
        pos = 0;
        if (std::isdigit(t[i]) && t[i] == '0')
        {
            pos = 1;
            len++;
        }
        else if (std::isdigit(t[i]) && t[i] == '1')
        {
            pos = 2;
            len++;
        }
        else if (std::isdigit(t[i]) && t[i] == '2')
        {
            pos = 3;
            len++;
        }
        else if (std::isdigit(t[i]) && ((t[i] == '3') || (t[i] == '4')))
        {
            pos = 4;
            len++;
        }
        else if (std::isdigit(t[i]) && t[i] == '5')
        {
            pos = 5;
            len++;
        }
        else if (t[i] == '.')
        {
            pos = 6;
            len = 0;
        }
        else if (std::isdigit(t[i]) && t[i] > '5' && t[i] <= '9')
        {
            pos = 7;
            len++;
        }
        prev = getIPV4State(prev, pos);
        if (prev == 0 || len > 3)
            return (false);
    }
    if (!std::isdigit(t[i - 1]))
        return false;
    return (true);
}

bool   Parser::check_port(std::string t)
{
    std::stringstream       parse;
    long long               n;
    std::string::iterator   it = t.begin();
    std::string::iterator   end = t.end();
    
    while (it != end)
    {
        if (!std::isdigit(*it))
            return false;
        ++it;
    }
    parse << t;
    parse >> n;
    if (n > 65535 || n < 0)
        return false;
    return true;
}

void    Parser::listenParser(std::vector<std::string>::iterator& it)
{
    if (*(it + 2) != ";")
        throw std::exception();
    ++it;
    std::string t = *it;
    size_t  pos = t.find(':');
    if (pos != std::string::npos)
    {
        std::string ip = t.substr(0, pos);
        std::string port = t.substr(pos + 1);
            
        if (ip.size() == 0 || port.size() == 0 || !check_ipv4(ip) || !check_port(port))
            throw std::exception();
        if (ip == "localhost")
            ip = "127.0.0.1";
        ++it;
        _listens.insert(std::pair<std::string, std::string>(ip, port));
    }
    else
    {    
        bool    ip = false;
        bool    port = false;
            
        ip = check_ipv4(t);
        port = check_port(t);
        if (ip)
        {
            if (t == "localhost")
                t = "127.0.0.1";
            _listens.insert(std::pair<std::string, std::string>(t, "80"));
            ++it;
            return ;
        }
        if (port)
        {
            _listens.insert(std::pair<std::string, std::string>("127.0.0.1", t));
            ++it;
            return ;
        }
        throw std::exception();
    }
}

int     Parser::getClientMaxBodySizeState(int prev, int pos)
{
    static int tokens[][3] = {
        {BD_ERR, BD_NUM, BD_ERR}, // 0 INI
        {BD_ERR, BD_NUM, BD_CHR}, // 1 BD_NUM
        {BD_ERR, BD_ERR, BD_ERR}  // 2 BD_CHR
    };
    return tokens[prev][pos];
}


bool    Parser::checkclientmaxbodysize(std::string t)
{
    int prev = 0;
    int pos;
    std::string::iterator   it = t.begin();
    std::string::iterator   end = t.end();

    for (; it != end; ++it)
    {
        pos = 0;
        if (std::isdigit(*it))
            pos = 1;
        else if (*it == 'g' || *it == 'G' || *it == 'k' || *it == 'K' || *it == 'm' || *it == 'M')
            pos = 2;
        prev = getClientMaxBodySizeState(prev, pos);
        if (prev == 0)
            return false;
    }
    return true;
}


void    Parser::clientmaxbodysizeParser(std::vector<std::string>::iterator& it)
{
    std::stringstream   parse;
    long long           n;

    if (*(it + 2) != ";")
        throw std::exception();
    ++it;
    if (checkclientmaxbodysize(*it) == false)
        throw std::exception();
    parse << *it;
    parse >> n;
    if (n < 0 || n > std::numeric_limits<int>::max())
        throw std::exception();
}

void    Parser::autoindexParser(std::vector<std::string>::iterator& it)
{
    if (*(it + 2) != ";")
        throw std::exception();
    ++it;
    if (*it != "on" && *it != "off")
        throw std::exception();
    ++it;
}

void	Parser::allowedParser(std::vector<std::string>::iterator& it)
{
	++it;
	while (*it != ";")
	{
		if (*it != "GET" || *it != "POST" || *it != "DELETE")
			throw std::exception();
		++it;
	}
}

int		Parser::getServerNameState(int prev, int pos)
{
	static int	matrix[][7] = {
		{1, 1, 2, 2, 2, 2, 1},
		{1, 1, 1, 1, 1, 1, 1}, //error
		{1, 3, 2, 2, 2, 2, 1}, //cualquier caracter
		{1, 1, 4, 2, 2, 2, 1}, //.
		{1, 3, 2, 5, 2, 2, 1}, //c
		{1, 3, 2, 2, 6, 2, 1}, //o
		{1, 3, 2, 2, 2, 2, 1}, //m
	};
	return (matrix[prev][pos]);
}

void	Parser::serverNameParser(std::vector<std::string>::iterator& it)
{
	std::string			str;
	int					prev = 0;
	int					pos;

	it++;
	while (*it != ";")
	{
		size_t		i = 0;
		str = *it;
		prev = 0;

		if (!check_ipv4(str))
		{
			while (i < str.length())
			{
				pos = 0;
				if (str[i] == '.')
        	        pos = 1;
				else if (str[i] == 'c')
					pos = 2;
				else if (str[i] == 'o')
					pos = 3;
				else if (str[i] == 'm')
					pos = 4;
				else if (std::isalpha(str[i]) && str[i] != '*')
					pos = 5;
				else
					pos = 6;
				prev = getServerNameState(prev, pos);
				if (prev == 1)
				{
					std::cout << "servernameparser1\n";
					throw std::exception();
				}
				i++;
			}
			if (prev != 6)
			{
				std::cout << "servernameparser1\n";
			throw std::exception();
			}
		}
		++it;
	}
}

int     Parser::getTypeOfItem(std::string& str)
{
    if (str.size() == 3 && strIsDigit(str) && (str[0] == '3' || str[0] == '4' || str[0] == '5'))
        return 1;
    if (str.size() == 4 && str[0] == '=' && strIsDigit(str.substr(1, 3)) && (str[1] == '1'
        || str[1] == '2'
        || str[1] == '3'
        || str[1] == '4'
        || str[1] == '5'))
    {
        return 2;
    }
    return 3;
}

int    Parser::getErrorPageParserState(int prev, int pos)
{
    static int matrix[][4] = {
        {EP_ERR, EP_COD, EP_ERR, EP_ERR}, // INI
        {EP_ERR, EP_COD, EP_OVR, EP_URI}, // EP_COD
        {EP_ERR, EP_ERR, EP_ERR, EP_URI}, // EP_OVR
        {EP_ERR, EP_ERR, EP_ERR, EP_ERR}  // EP_URI
    };
    
    return matrix[prev][pos];
}

void    Parser::errorpageParser(std::vector<std::string>::iterator& it)
{
    int pos;
    int prev = 0;

    ++it;
    while (*it != ";")
    {
        pos = getTypeOfItem(*it);
        prev = getErrorPageParserState(prev, pos);
        if (prev == 0)
            throw std::exception();
        ++it;
    }
    if (getTypeOfItem(*(it - 1)) != 3)
        throw std::exception();
}

int		Parser::getIndexState(int prev, int pos)
{
	static int	matrix[][9] = {
		{1, 1, 10, 10, 10, 10, 10, 10},
		{1, 1,  1,  1,  1,  1,  1,  1}, //1 error
		{1, 1,  3,  6,  1,  1,  1,  1}, //2 .
		{1, 1,  1,  4,  1,  1,  1,  1}, //3 p
		{1, 1,  5,  1,  1,  1,  1,  1}, //4 h
		{1, 1,  1,  1,  1,  1,  1,  1}, //5 p2
		{1, 1,  1,  1,  7,  1,  1,  1}, //6 h2
		{1, 1,  1,  1,  1,  8,  1,  1}, //7 t
		{1, 1,  1,  1,  1,  1,  9,  1}, //8 m
		{1, 1,  1,  1,  1,  1,  1,  1}, //9 l
		{1, 2, 10, 10, 10, 10, 10, 10}, //10 alpha
	//	 e  .  p  h  t  m  l  a
	};
	return (matrix[prev][pos]);
}


void	Parser::chooseIndexState(std::string str)
{
	std::string::iterator	it = str.begin();
	std::string::iterator	end= str.end();
	int	pos;
	int	prev = 0;
	while (it != end)
	{
		pos = 0;
		if (*it == '.')
			pos = 1;
		else if (*it == 'p')
			pos = 2;
		else if (*it ==  'h')
			pos = 3;
		else if (*it == 't')
			pos = 4;
		else if (*it == 'm')
			pos = 5;
		else if (*it == 'l')
			pos = 6;
		else if (std::isalpha(*it))
			pos = 7;
		prev = getIndexState(prev, pos);
		if (prev == 1)
			throw std::exception();
		it++;
	}
	if (prev != 5 && prev != 8 && prev != 9)
			throw std::exception();
}

void	Parser::indexParser(std::vector<std::string>::iterator& it)
{
	std::string			str;

	++it;
	while (*it != ";")
	{
		str = *it;
		//std::cout << str;
		chooseIndexState(str);
		++it;
	}
	
}
Parser::Parser(const char* in_file)
{
	std::ifstream   config_file(in_file);
    std::vector<std::string>::iterator  it;
    std::vector<std::string>::iterator  end;

    if (!config_file.is_open())
        throw std::runtime_error("Could not open config file\n");
    //tokenizar y parsear
    this->rmComments(config_file);
    this->tokenize();
    if (this->chooseState(this->_tokens))
        ;
    it = _tokens.begin();
    end = _tokens.end();

    for (; it != end; ++it)
    {
        if (*it == "listen")
            this->listenParser(it);
        else if (*it == "autoindex")
            this->autoindexParser(it);
        else if (*it == "allowed_methods")
            this->allowedParser(it);
		else if (*it == "server_name")
			this->serverNameParser(it);
		else if (*it == "client_max_body_size")
			this->clientmaxbodysizeParser(it);
        else if (*it == "error_page")
            this->errorpageParser(it);
        else if (*it == "index")
            this->indexParser(it);
    }
    /*for (size_t i = 0; i < this->_tokens.size(); i++)
       std::cout << this->_tokens[i] << std::endl;
    for (size_t i = 0; i < this->_config_file.size(); i++)
       std::cout << this->_config_file[i];*/
       
    config_file.close();
}

Parser::~Parser()
{
    _tokens.clear();
    _listens.clear();
    _lflags.clear();
}

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
