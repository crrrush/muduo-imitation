
#include <iostream>
#include <string>
#include <regex>

using namespace std;

int main()
{
    // HTTP请求行格式： GET /aaabbbccc/login?user=username&pass=123123 HTTP/1.1\r\n
    string head = "GET /aaabbbccc/login?user=username&pass=123123 HTTP/1.1";
    // regex re("(GET|HEAD|POST|PUT|DELETE) ([^?]*)\\?(.*) (HTTP/1\\.[01])(?:\n|\r\n)?");
    regex re("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?");
    // GET|HEAD|POST|PUT|DELETE 表示匹配并提取其中任意一个字符串
    // [^?]*  [^?]匹配非？字符 后边的*表示0次或多次
    // \\?(.*) \\? 表示原始的？字符     (.*)表示提取该符号之前的字符也就是？之后的任意字符0次或多次，直到遇到空格
    // [01]匹配两个数字字符之中的一个
    // (?:\n|\r\n)? （?:...）表示匹配某个格式字符串，但是不提取 最后的？表示匹配0次或1次
    smatch sm;
    regex_match(head, sm, re);

    for(const auto& e : sm) cout<< e << endl;

    return 0;
}