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
    curly = 0
    for ch in s:
        if ch == '{':
            if token:
                tokens.append(token)
                token = ''
            tokens.append('{')
            curly += 1
            # mimic original: if curly_braces > 2 return error (but we continue)
        elif ch == '}':
            if token:
                tokens.append(token)
                token = ''
            tokens.append('}')
            curly -= 1
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


def is_config_word(tok):
    return tok in CONFIG_KEYS


def getState(prev, pos):
    tokens = [
        [0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0],
        [0, 0, 0, 0, 5, 0, 0, 0, 9, 0, 0],
        [0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [0, 0, 0, 0, 0, 6, 0, 0, 0, 0, 0],
        [0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0],
        [0, 0, 0, 0, 0, 0, 0, 8, 9, 0, 0],
        [0, 0, 0, 4, 5, 0, 0, 0, 9, 0, 0],
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0],
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 11],
        [0, 0, 0, 4, 5, 0, 0, 8, 9, 0, 0],
    ]
    return tokens[prev][pos]


def choose_state(tokens):
    prev = 0
    is_serv = False
    is_loc = False
    for i in range(len(tokens)):
        tok = tokens[i]
        pos = 0
        if tok == 'server':
            pos = 1
        elif tok == '{' and prev == 2:
            pos = 2
            is_serv = True
        elif tok == '{' and prev == 6:
            pos = 6
            is_loc = True
        elif tok == '}':
            if (prev == 3 or prev == 8 or prev == 11) and is_serv:
                pos = 3
                if not is_loc:
                    is_serv = False
            if (prev == 7 or prev == 11) and is_loc:
                pos = 7
                is_loc = False
        elif tok == 'location':
            pos = 4
        elif is_config_word(tok):
            pos = 8
        elif tok == ';':
            pos = 10
        else:
            pos = 9
            if prev == 5:
                # mimic check: if tokens[i] == '=' and tokens[i+1] != '{' then i++
                if tok == '=' and i + 1 < len(tokens) and tokens[i+1] != '{':
                    # consume next token in C++ code by skipping; here just emulate by advancing i in outer loop
                    # but cannot modify for-loop index easily — we will replicate behaviour by returning state change
                    pass
                elif tok == '=' and i + 1 < len(tokens) and tokens[i+1] == '{':
                    return (1, i, tok)
                pos = 5
        prev = getState(prev, pos)
        if prev == 1:
            return (1, i, tok)
    return (0, -1, '')


def main():
    if len(sys.argv) < 2:
        path = 'configs/default.conf'
    else:
        path = sys.argv[1]
    if not os.path.exists(path):
        print('Config not found:', path)
        sys.exit(1)
    with open(path, 'r') as f:
        raw = f.read()
    cleaned = rm_comments(raw)
    tokens = tokenize(cleaned)
    # print tokens length and sample
    print('Tokens count:', len(tokens))
    # show first 120 tokens
    print('First tokens:', tokens[:120])
    res = choose_state(tokens)
    if res[0] == 1:
        idx = res[1]
        print('Parser state machine reported error at token index', idx)
        context = tokens[max(0, idx-5):min(len(tokens), idx+6)]
        print('Context tokens (around error):')
        for j, t in enumerate(context, start=max(0, idx-5)):
            mark = '<--' if j == idx else ''
            print('{:4d}: {:20} {}'.format(j, t, mark))
    else:
        print('No syntax error detected by chooseState')

if __name__ == '__main__':
    main()
