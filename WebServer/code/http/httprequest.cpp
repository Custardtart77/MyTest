#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", };

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  };

// 初始化请求对象信息
void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";  // 定义的变量初始化为空
    state_ = REQUEST_LINE;                    // 初始化请求状态：正在解析请求行
    header_.clear();                          // 对应的哈希集合清零，下同
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

// 解析请求数据，这个函数是解析报文的核心函数
// 解析的过程涉及了有限状态机的转移
bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n"; // 行结束符
    if(buff.ReadableBytes() <= 0) {
        return false;
    }
    // buff中有数据可读，并且状态没有到FINISH，就一直解析
    while(buff.ReadableBytes() && state_ != FINISH) {
        // 获取一行数据，根据\r\n为结束标志
        // lineEnd 为一行结束时的下一个
        // 一行一行地解析
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.Peek(), lineEnd);     // 获取一行数据

        switch(state_)
        {
        case REQUEST_LINE:
            // 解析请求首行
            if(!ParseRequestLine_(line)) {
                return false;
            }
            // 解析出请求资源路径
            ParsePath_();
            break;    
        case HEADERS:
            // 解析请求头
            ParseHeader_(line);
            if(buff.ReadableBytes() <= 2) {
                state_ = FINISH;
            }
            break;
        case BODY:
            // 解析请求体
            ParseBody_(line);
            break;
        default:
            break;
        }
        // 判断数据读取完了，说明解析完了
        if(lineEnd == buff.BeginWrite()) { break; }
        // 读完一行，移动读指针
        buff.RetrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

// 解析请求路径
void HttpRequest::ParsePath_() {
    // 如果访问根目录，默认表示访问index.html
    // 例如 http://192.168.110.111:10000/
    if(path_ == "/") {
        path_ = "/index.html"; 
    }
    else {
        // 其他默认的一些页面
        // 例如 http://192.168.110.111:10000/regist
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}
// 解析请求行
bool HttpRequest::ParseRequestLine_(const string& line) {
    // GET / HTTP/1.1
    // 正则匹配
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    smatch subMatch;   // 传出参数
    if(regex_match(line, subMatch, patten)) {   
        // 解析成功，然后获取请求类型，路径，版本，然后进行状态转移
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9
// Connection: keep-alive
// 解析请求头
void HttpRequest::ParseHeader_(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        // 匹配成功，将请求头的键和值放入unordered_map中
        header_[subMatch[1]] = subMatch[2];
    }
    else {
        // 若匹配不成功，说明请求头读取完，开始处理解析请求体
        state_ = BODY;
    }
}

// 解析请求体
void HttpRequest::ParseBody_(const string& line) {
    body_ = line;  // 将数据保存至请求体
    ParsePost_();  // 解析post数据
    state_ = FINISH;  // 解析结束
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

// 将十六进制的字符，转换成十进制的整数
int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    return ch;
}

void HttpRequest::ParsePost_() {
    // 这里只解析了单一的post请求
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        // 解析表单信息，解析出用户名和密码
        ParseFromUrlencoded_();
        // 判断是否位于登录或者注册页面
        if(DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                // 0为注册，1为登录
                bool isLogin = (tag == 1);
                // 验证是否登录成功，或者注册是否成功，UserVerify内部也实现了注册的功能
                // 注册成功也会自动登录然后来到欢迎界面
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {
                    path_ = "/error.html";
                }
            }
        }
    }   
}

void HttpRequest::ParseFromUrlencoded_() {
    if(body_.size() == 0) { return; }
    // username=zhangsan&password=123
    string key, value;
    int num = 0;
    int n = body_.size();   // 整个请求体的大小
    int i = 0, j = 0;
    // 然后逐个字符去遍历，根据=、&号去分割信息
    for(; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            // 简单的加密的操作，编码
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            // 将键和值写入日志
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

// 用户验证（整合了登录和注册的验证）
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    if(name == "" || pwd == "") { return false; }       //若用户名和密码都为空，则返回假
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    // 新建，然后自动析构，RAII机制实现数据库连接池
    SqlConnRAII(&sql,  SqlConnPool::Instance());
    assert(sql);
    // 登录为true, 注册为false
    bool flag = false;
    unsigned int j = 0;
    char order[256] = { 0 };        // 用于保存sql语句
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    
    if(!isLogin) { flag = true; }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    // 执行由“Null终结的字符串”查询指向的SQL查询。如果查询成功，返回0。如果出现错误，返回非0值。
    if(mysql_query(sql, order)) { 
        // 若查询失败

        /* 释放由mysql_store_result()、mysql_use_result()、mysql_list_dbs()等为结果集分配的内存。
        完成对结果集的操作后，必须调用mysql_free_result()释放结果集使用的内存。
        释放完成后，不要尝试访问结果集。*/
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);      // 将查询的全部结果读取到客户端
    j = mysql_num_fields(res);          // 返回结果集中的行数
    fields = mysql_fetch_fields(res);   // 对于结果集，返回所有MYSQL_FIELD结构的数组。每个结构提供了结果集中1列的字段定义

    // 检索结果集的下一行。在mysql_store_result()之后使用时，如果没有要检索的行，mysql_fetch_row()返回NULL
    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if(isLogin) {
            if(pwd == password) { flag = true; }
            else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } 
        else { 
            flag = false; 
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);     // 释放内存中的结果

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        // 注册成功
        flag = true;
    }
    SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}