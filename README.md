>>Web Bench is very simple tool for benchmarking WWW or proxy servers. Uses fork() for simulating multiple clients and can use HTTP/0.9-HTTP/1.1 requests. This benchmark is not very realistic, but it can test if your HTTPD can realy handle that many clients at once (try to run some CGIs) without taking your machine down. Displays pages/min and bytes/sec. Can be used in more aggressive mode with -f switch.
>>  The introduction is from official site http://home.tiscali.cz/~cz210552/webbench.html.<br/>
>>  However, Web Bench doesn't support post method. Thus, I modify the file webbench.c to add this method. The parameters now are:<br/>
>> <br/> 
>>>>  webbench [option]... URL<br/>
>>>>  -f|--force               Don't wait for reply from server.<br/>
>>>>  -r|--reload              Send reload request - Pragma: no-cache.<br/>
>>>>  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.<br/>
>>>>  -p|--proxy <server:port> Use proxy server for request.<br/>
>>>>  -c|--clients <n>         Run <n> HTTP clients at once. Default one.<br/>
>>>>  -9|--http09              Use HTTP/0.9 style requests.<br/>
>>>>  -1|--http10              Use HTTP/1.0 protocol.<br/>
>>>>  -2|--http11              Use HTTP/1.1 protocol.<br/>
>>>>  --get                    Use GET request method.<br/>
>>>>  --head                   Use HEAD request method.<br/>
>>>>  --options                Use OPTIONS request method.<br/>
>>>>  --trace                  Use TRACE request method.<br/>
>>>>  --post                   Use POST request method.<br/>
>>>>  -?|-h|--help             This information.<br/>
>>>>  -V|--version             Display program version.<br/>
>>  
>> <br/> 
>>  when using --post,it is like:
>>    
>> <br/> 
>>    `--post="user=codeyz&password=12345"`
>> <br/> 
