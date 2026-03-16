#!/bin/bash

# --------------------------
# CGI Response Header
# --------------------------
echo "Content-Type: text/html"
echo ""

# --------------------------
# HTML Header
# --------------------------
echo "<html><head><title>Bash CGI Debug</title></head><body>"
echo "<h1>Bash CGI Script Executed</h1>"

method="${REQUEST_METHOD:-GET}"
echo "<p><strong>Request Method:</strong> $method</p>"

echo "<hr>"
echo "<h2>CGI Runtime Environment</h2>"
echo "<ul>"
echo "<li><strong>SCRIPT_NAME:</strong> $SCRIPT_NAME</li>"
echo "<li><strong>QUERY_STRING:</strong> $QUERY_STRING</li>"
echo "<li><strong>CONTENT_LENGTH:</strong> ${CONTENT_LENGTH:-0}</li>"
echo "<li><strong>CONTENT_TYPE:</strong> ${CONTENT_TYPE:-none}</li>"
echo "<li><strong>SERVER_PROTOCOL:</strong> $SERVER_PROTOCOL</li>"
echo "<li><strong>HTTP_USER_AGENT:</strong> $HTTP_USER_AGENT</li>"
echo "</ul>"

echo "<hr>"


# ----------------------------------
# GET Processing
# ----------------------------------
if [ "$method" = "GET" ]; then
    echo "<h2>GET Data</h2>"

    if [ -n "$QUERY_STRING" ]; then
        echo "<p><strong>Raw Query:</strong> $QUERY_STRING</p>"

        echo "<h3>Parsed Params:</h3><ul>"
        # Convert QUERY_STRING like a=b&c=d into key/value pairs
        IFS='&' read -ra pairs <<< "$QUERY_STRING"
        for p in "${pairs[@]}"; do
            key="${p%%=*}"
            val="${p#*=}"
            echo "<li>${key} = ${val}</li>"
        done
        echo "</ul>"
    else
        echo "<p>No query string received.</p>"
    fi
fi


# ----------------------------------
# POST Processing
# ----------------------------------
if [ "$method" = "POST" ]; then
    echo "<h2>POST Data</h2>"

    # Read exactly CONTENT_LENGTH bytes from stdin
    if [ -n "$CONTENT_LENGTH" ] && [ "$CONTENT_LENGTH" -gt 0 ] 2>/dev/null; then
        body="$(dd bs=1 count="$CONTENT_LENGTH" 2>/dev/null)"
        echo "<p><strong>Raw Body:</strong> $body</p>"

        echo "<h3>Parsed Params:</h3><ul>"
        IFS='&' read -ra pairs <<< "$body"
        for p in "${pairs[@]}"; do
            key="${p%%=*}"
            val="${p#*=}"
            echo "<li>${key} = ${val}</li>"
        done
        echo "</ul>"
    else
        echo "<p>No POST body received.</p>"
    fi
fi


echo "<hr>"

echo "<h2>Execution Summary</h2>"
echo "<p>This CGI script was successfully executed by your server.  
It inspected environment variables to understand how the HTTP request  
was translated into CGI parameters. It also read input data from stdin  
(if POST) and parsed any query or form fields.</p>"

echo "<p>This helps debug CGI integration and ensures your server  
correctly forwards headers, query strings, and POST bodies to CGI scripts.</p>"

echo "<hr>"
echo "<a href=\"/\">Back to Home</a>"
echo "<p><a href=\"/cgi-bin\">Go back</a></p>"

echo "</body></html>"
