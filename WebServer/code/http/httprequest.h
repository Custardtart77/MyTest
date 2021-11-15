#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>     
#include <mysql/mysql.h>  //mysql

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"

class HttpRequest {
public:
    // 有限状态机，用于解析请求
    // 主状态机的状态
    enum PARSE_STATE {
        REQUEST_LINE,   // 正在解析请求首行
        HEADERS,        // 正在解析请求头
        BODY,           // 正在解析请求体
        FINISH,         // 解析完成
    };

    enum HTTP_CODE {
        /*
        NO_REQUEST = 0, 请求不完整，需要继续读取客户请求
        GET_REQUEST,    表示获得了一个完整的客户请求
        BAD_REQUEST,    表示客户请求语法错误
        NO_RESOURSE,    表示服务器没有资源
        FORBIDDENT_REQUEST, 表示客户对资源没有足够的访问权限
        FILE_REQUEST,   文件请求，获取文件成功
        INTERNAL_ERROR, 表示服务器内部错误
        CLOSED_CONNECTION,  表示客户端已经关闭连接了
        */
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };
    // 构造函数执行初始化操作
    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void Init();
    bool parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

private:
    
    bool ParseRequestLine_(const std::string& line);    // 解析请求行
    void ParseHeader_(const std::string& line);         // 解析请求头
    void ParseBody_(const std::string& line);           // 解析请求体（请求数据）

    void ParsePath_();              // 解析请求的路径
    void ParsePost_();              // 解析post请求？
    void ParseFromUrlencoded_();    // 解析表单数据

    // 验证用户注册登录
    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);

    PARSE_STATE state_;     // 解析的状态
    std::string method_, path_, version_, body_;    // 请求方法，请求路径，协议版本，请求体
    std::unordered_map<std::string, std::string> header_;   // 请求头
    std::unordered_map<std::string, std::string> post_;     // post请求表单数据

    static const std::unordered_set<std::string> DEFAULT_HTML;  // 默认的网页
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG; 
    static int ConverHex(char ch);  // 将十六进制字符转换成十进制整数
};


#endif //HTTP_REQUEST_H