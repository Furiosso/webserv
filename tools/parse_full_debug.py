#!/usr/bin/env python3
import sys
import os

CONFIG_KEYS = [
    "listen",
    "server_name",
    "error_page",
    "client_max_body_size",
    "root",
    "location",
    "index",
    "autoindex",
    "return",
    "upload_store",
    "upload_pass",
    "cgi",
    "alias",
    "allowed_methods"
]

CGI_KEYS = ['.py', '.php', '.pl', '.bla']


def rm_comments(text):
    out = []
    for line in text.splitlines():
        if not line.strip():
            out.append(' ')
            continue
        pos = line.find('#')
        if pos != -1:
            line = line[:pos]
        clean = line.rstrip()
        if clean:
            out.append(clean + ' ')
    return ''.join(out)


def tokenize(s):
    tokens = []
    token = ''
    for ch in s:
        if ch == '{':
            if token:
                tokens.append(token)
                token = ''
            tokens.append('{')
        elif ch == '}':
            if token:
                tokens.append(token)
                token = ''
            tokens.append('}')
        elif ch == ';':
            if token:
                tokens.append(token)
                token = ''
            tokens.append(';')
        elif ch in ' \t\n\r':
            if token:
                tokens.append(token)
                token = ''
        else:
            token += ch
    if token:
        tokens.append(token)
    return tokens


def is_cgi_word(tok):
    return tok in CGI_KEYS


def check_listen(tokens, idx):
    # tokens[idx] == 'listen'
    if idx + 2 >= len(tokens) or tokens[idx+2] != ';':
        return False, 'listen: missing ";" or malformed'
    t = tokens[idx+1]
    if ':' in t:
        ip, port = t.split(':', 1)
        if not ip or not port:
            return False, 'listen: empty ip or port'
        # basic numeric check for port
        if not all(c.isdigit() or c=='.' for c in ip) and ip != 'localhost':
            # allow domain names? Parser expects ipv4 or localhost
            pass
        if not port.isdigit():
            return False, 'listen: port not numeric'
    else:
        if not (t.isdigit() or t == 'localhost'):
            # could be ip or port
            if t.count('.')==3:
                # naive ip check
                pass
            else:
                return False, 'listen: token not ip or port'
    return True, None


def check_client_max_body_size(tokens, idx):
    if idx + 2 >= len(tokens) or tokens[idx+2] != ';':
        return False, 'client_max_body_size: missing ";"'
    val = tokens[idx+1]
    # naive check
    if not val[0].isdigit():
        return False, 'client_max_body_size: invalid value'
    return True, None


def check_autoindex(tokens, idx):
    if idx + 2 >= len(tokens) or tokens[idx+2] != ';':
        return False, 'autoindex: missing ";"'
    if tokens[idx+1] not in ('on','off'):
        return False, 'autoindex: must be on/off'
    return True, None


def check_allowed_methods(tokens, idx):
    # allowed_methods <list> ;
    j = idx + 1
    if j >= len(tokens):
        return False, 'allowed_methods: missing values'
    while j < len(tokens) and tokens[j] != ';':
        if tokens[j] not in ('GET','POST','DELETE'):
            return False, 'allowed_methods: unsupported method '+tokens[j]
        j += 1
    if j >= len(tokens) or tokens[j] != ';':
        return False, 'allowed_methods: missing ;'
    return True, None


def check_server_name(tokens, idx):
    # require ; after names
    j = idx + 1
    if j >= len(tokens):
        return False, 'server_name: missing token'
    while j < len(tokens) and tokens[j] != ';':
        j += 1
    if j >= len(tokens) or tokens[j] != ';':
        return False, 'server_name: missing ;'
    return True, None


def check_cgi(tokens, idx):
    # mimic C++: ++it; str = *it; if (*(it+2) != ";" || !isCgiWord(str)) throw
    if idx + 3 >= len(tokens):
        return False, 'cgi: too few tokens'
    ext = tokens[idx+1]
    exe = tokens[idx+2]
    if tokens[idx+3] != ';':
        return False, 'cgi: missing ; at end'
    if not is_cgi_word(ext):
        return False, 'cgi: unsupported extension '+ext
    # check executable exists and is executable
    if not os.path.exists(exe):
        return False, 'cgi: executable not found '+exe
    if not os.access(exe, os.X_OK):
        return False, 'cgi: executable not executable '+exe
    return True, None


def check_root(tokens, idx):
    if idx + 2 >= len(tokens) or tokens[idx+2] != ';':
        return False, 'root: missing ; or malformed'
    return True, None


def check_error_page(tokens, idx):
    # simplified: ensure ; exists and last token before ; is uri
    j = idx + 1
    if j >= len(tokens):
        return False, 'error_page: missing tokens'
    while j < len(tokens) and tokens[j] != ';':
        j += 1
    if j >= len(tokens) or tokens[j] != ';':
        return False, 'error_page: missing ;'
    return True, None


def simulate(tokens):
    # first pass: count servers
    server_count = tokens.count('server')
    print('server_count:', server_count)
    # create servers list placeholders
    servers = [{} for _ in range(server_count)]
    # First pass checks
    i = 0
    idx = 0
    while idx < len(tokens):
        tok = tokens[idx]
        if tok == 'server':
            i += 1
        elif tok == 'listen':
            ok,msg = check_listen(tokens, idx)
            if not ok:
                return False, idx, msg
            idx += 2
        elif tok == 'autoindex':
            ok,msg = check_autoindex(tokens, idx)
            if not ok:
                return False, idx, msg
            # advance to ;
            while idx < len(tokens) and tokens[idx] != ';': idx += 1
        elif tok == 'allowed_methods':
            ok,msg = check_allowed_methods(tokens, idx)
            if not ok:
                return False, idx, msg
            # move idx to ;
            while idx < len(tokens) and tokens[idx] != ';': idx += 1
        elif tok == 'server_name':
            ok,msg = check_server_name(tokens, idx)
            if not ok:
                return False, idx, msg
            while idx < len(tokens) and tokens[idx] != ';': idx += 1
        elif tok == 'client_max_body_size':
            ok,msg = check_client_max_body_size(tokens, idx)
            if not ok:
                return False, idx, msg
            idx += 2
        elif tok == 'error_page':
            ok,msg = check_error_page(tokens, idx)
            if not ok:
                return False, idx, msg
            # advance to ;
            while idx < len(tokens) and tokens[idx] != ';': idx += 1
        elif tok == 'index':
            # skip to ;
            while idx < len(tokens) and tokens[idx] != ';': idx += 1
        elif tok == 'cgi':
            ok,msg = check_cgi(tokens, idx)
            if not ok:
                return False, idx, msg
            idx += 3
        elif tok == 'root':
            ok,msg = check_root(tokens, idx)
            if not ok:
                return False, idx, msg
            idx += 2
        elif tok == 'alias':
            # top-level alias not supported
            return False, idx, 'alias directive not supported at top level'
        elif tok == 'location':
            # skip forward until matching '}' (first pass in C++ does this)
            j = idx
            depth = 0
            while j < len(tokens):
                if tokens[j] == '{': depth += 1
                elif tokens[j] == '}':
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            if j >= len(tokens):
                return False, idx, 'location: unmatched braces'
            idx = j
        idx += 1
    # Second pass: parse locations
    idx = 0
    server_idx = -1
    while idx < len(tokens):
        tok = tokens[idx]
        if tok == 'server':
            server_idx += 1
        elif tok == 'location':
            # parse location block starting at idx; next token is path, then '{', then directives until matching '}'
            if idx + 1 >= len(tokens):
                return False, idx, 'location: missing path'
            path = tokens[idx+1]
            # find '{' at idx+2 normally
            if idx+2 >= len(tokens) or tokens[idx+2] != '{':
                return False, idx, 'location: missing { after path'
            j = idx+3
            while j < len(tokens) and tokens[j] != '}':
                d = tokens[j]
                if d == 'root':
                    ok,msg = check_root(tokens, j)
                    if not ok:
                        return False, j, 'location root: '+msg
                    j += 3 if j+3 < len(tokens) and tokens[j+3]==';' else 3
                elif d == 'index':
                    # skip to ;
                    while j < len(tokens) and tokens[j] != ';': j += 1
                elif d == 'cgi':
                    ok,msg = check_cgi(tokens, j)
                    if not ok:
                        return False, j, 'location cgi: '+msg
                    j += 3
                elif d == 'autoindex':
                    ok,msg = check_autoindex(tokens, j)
                    if not ok:
                        return False, j, 'location autoindex: '+msg
                    j += 3
                elif d == 'allowed_methods':
                    ok,msg = check_allowed_methods(tokens, j)
                    if not ok:
                        return False, j, 'location allowed_methods: '+msg
                    while j < len(tokens) and tokens[j] != ';': j += 1
                elif d == 'alias':
                    # alias in location: rootLocParser with n==1 uses throw when n==1 -> but C++ supports alias in location via rootLocParser(it, loc, 1)
                    # here check pattern alias <path> ;
                    if j+2 >= len(tokens) or tokens[j+2] != ';':
                        return False, j, 'location alias: missing ;'
                    j += 3
                elif d == 'client_max_body_size':
                    ok,msg = check_client_max_body_size(tokens, j)
                    if not ok:
                        return False, j, 'location client_max_body_size: '+msg
                    j += 3
                elif d == 'error_page':
                    ok,msg = check_error_page(tokens, j)
                    if not ok:
                        return False, j, 'location error_page: '+msg
                    while j < len(tokens) and tokens[j] != ';': j += 1
                elif d == ';':
                    j += 1
                else:
                    return False, j, 'location: unexpected token '+d
            if j >= len(tokens):
                return False, idx, 'location: missing closing }'
            idx = j
        idx += 1
    return True, -1, None


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'configs/default.conf'
    if not os.path.exists(path):
        print('config not found', path)
        sys.exit(1)
    raw = open(path).read()
    cleaned = rm_comments(raw)
    tokens = tokenize(cleaned)
    print('Tokens count:', len(tokens))
    ok, pos, msg = simulate(tokens)
    if ok:
        print('No parser-simulated error detected')
    else:
        print('Parser-simulated error at token index', pos)
        context = tokens[max(0,pos-6):min(len(tokens), pos+6)]
        for i,t in enumerate(context, start=max(0,pos-6)):
            mark = '<--' if i==pos else ''
            print('{:4d}: {:30} {}'.format(i, t, mark))
        print('Reason:', msg)

if __name__ == "__main__":
    main()
