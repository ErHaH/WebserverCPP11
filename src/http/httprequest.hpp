#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <regex>

#include "../buffer/buffer.hpp"
#include "../pool/sqlconnpool.hpp"
#include "../pool/sqlconnRAII.hpp"


class HttpRequest {
public:
    //请求状态机
    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    //报文解析状态机
    enum PARSE_STATE {
        REQUEST_LINE,
        REQUEST_HEADER,
        REQUEST_BODY,
        REQUEST_FINISH
    };

    HttpRequest();
    ~HttpRequest();
    bool ParseRequest(Buffer& buf);
    bool IsKeepAlive() const;

    std::string GetPath() const;
    std::string& GetPath();
    std::string GetMethod() const;
    std::string GetVersion() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

private:
    bool ParseRequestLine_(const std::string& line);
    void ParseRequestHeader_(const std::string& line);
    void ParseRequestBody_(const std::string& line);
    void ParsePath_();
    void ParsePost_();
    void ParseFromUrlencoded_();
    bool UserVerify_(const std::string& user, const std::string& pw, bool isLogin);
    static int ConverHex(const char ch);        //十六转十进制

private:
    PARSE_STATE state_;
    std::string method_, path_, version_, body_;
    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;                         //请求体账号密码等

    static const std::unordered_set<std::string> DEFAULT_HTML_;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG_;
};

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML_ {
    "/index", "/register", "/login", "/welcome", "/video", "/picture"
};

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG_ {
    {"/register.html", 0}, {"/login.html", 1}
};

HttpRequest::HttpRequest() {
    method_ = "";
    path_ = "";
    version_ = "";
    header_.clear();
    post_.clear();
}

HttpRequest::~HttpRequest() {
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

bool HttpRequest::ParseRequest(Buffer& buf) {
    const char CRLF[] = "\r\n";
    if (buf.ReadableBytes() <= 0) {
        return false;
    }
    while(buf.ReadableBytes() > 0 && state_ != REQUEST_FINISH) {
        const char* lineEnd = std::search(buf.Peek(), buf.BeginWriteConst(), CRLF, CRLF + 2);
        std::string line(buf.Peek(), lineEnd);
        switch (state_)
        {
        case REQUEST_LINE:
            if (ParseRequestLine_(line) == false) {
                return false;
            }
            ParsePath_();
            break;
        case REQUEST_HEADER:
            ParseRequestHeader_(line);
            if (buf.ReadableBytes() <= 2) {
                state_ = REQUEST_FINISH;
            }
            break;     
        case REQUEST_BODY:
            ParseRequestBody_(line);
            break;                              
        default:
            break;
        }
        if (lineEnd == buf.BeginWrite()) {
            state_ = REQUEST_FINISH;
            break;
        }
        buf.RetrieveUntil(lineEnd + 2);
    }
    return true;
}

bool HttpRequest::ParseRequestLine_(const std::string &line) {
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch regResult;
    if (std::regex_match(line, regResult, patten)) {
        method_ = regResult[1];
        path_ = regResult[2];
        version_ = regResult[3];
        state_ = REQUEST_HEADER;
        return true;
    }
    return false;
}

void HttpRequest::ParsePath_() {
    if(path_ == "/") {
        path_ = "/index.html";
    }
    else {
        for(auto& item : DEFAULT_HTML_) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

void HttpRequest::ParseRequestHeader_(const std::string &line) {
    std::regex patten("^([^:]+): ?(.*)$");
    std::smatch regResult;
    if (std::regex_match(line, regResult, patten)) {
        header_[regResult[1]] = regResult[2];
    }
    else {
        state_ = REQUEST_BODY;
    }
}

void HttpRequest::ParseRequestBody_(const std::string &line) {
    body_ = line;
    ParsePost_();
    state_ = REQUEST_FINISH;
}

void HttpRequest::ParsePost_() {
    if((method_ == "POST" || method_ == "post") && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded_();
        if(DEFAULT_HTML_TAG_.find(path_) != DEFAULT_HTML_TAG_.end()) {
            int tag = DEFAULT_HTML_TAG_.find(path_)->second;
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if(UserVerify_(post_["username"], post_["password"], isLogin)) {
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
    if (body_.size() == 0) return;
    std::string key, value;
    int num = 0;
    int len = body_.size();
    int rightPos = 0, leftPos = 0;

    for(; rightPos < len; ++rightPos) {
        char ch = body_[rightPos];
        switch (ch)
        {
        case '=':
            key = body_.substr(leftPos, rightPos - leftPos);
            leftPos = rightPos + 1;
            break;
        case '+':
            body_[rightPos] = ' ';
            break;        
        case '%':
            num = ConverHex(body_[rightPos + 1]) * 16 + ConverHex(body_[rightPos + 2]);
            body_[rightPos + 2] = num % 10 + '0';
            body_[rightPos + 1] = num / 10 + '0';
            rightPos += 2;
            break;
        case '&':
            value = body_.substr(leftPos, rightPos - leftPos);
            leftPos = rightPos + 1;
            post_[key] = value;
            break;   
        default:
            break;
        }
    }
    //处理最后一个键值表单字段
    assert(leftPos <= rightPos);
    if(post_.find(key) != post_.end() && leftPos < rightPos) {
        value = body_.substr(leftPos, rightPos - leftPos);
        post_[key] = value;
    }
}

int HttpRequest::ConverHex(char ch) {
    if(ch >= 'A' && ch <= 'Z') return ch - 'A' + 10;
    if(ch >= 'a' && ch <= 'z') return ch - 'a' + 10;
}

bool HttpRequest::UserVerify_(const std::string &user, const std::string &pw, bool isLogin) {
    if(user == "" || pw == "") return false;
    MYSQL* sql = nullptr;
    SqlConnRAII(&sql, SqlConnPool::GetInstance());
    assert(sql);

    bool flag = false;
    unsigned int fieldsNum = 0;
    char order[256] = { 0 };
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;
    //todo
    //存在sql注入问题
    std::string selectStatement = "SELECT username, password FROM user WHERE username='%s' LIMIT 1";
    std::string insertStatement = "INSERT INTO user(username, password) VALUES('%s','%s')";

    if(!isLogin) flag = true;
    snprintf(order, sizeof(order) / sizeof(order[0]), selectStatement.c_str(), user.c_str());
    if (mysql_query(sql, order)) {
        mysql_free_result(res);
        return false;
    }

    res = mysql_store_result(sql);
    fieldsNum = mysql_num_fields(res);
    fields = mysql_fetch_field(res);

    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        if(isLogin) {
            std::string passwordInDB(row[1]);
            if(passwordInDB == pw) {
                flag = true;
            }
            else {
                flag = false;
            }
        }
    }
    mysql_free_result(res);

    if(!isLogin && flag == true) {
        bzero(order, sizeof(order) / sizeof(order[0]));
        snprintf(order, sizeof(order) / sizeof(order[0]), insertStatement.c_str(), user.c_str(), pw.c_str());
        if (mysql_query(sql, order)) {
            flag = false;
        }
        flag = true;
    }

    SqlConnPool::GetInstance()->FreeSqlConn(sql);
    return flag;
}


std::string HttpRequest::GetPath() const {
    return path_;
}

std::string& HttpRequest::GetPath() {
    return path_;
}

std::string HttpRequest::GetMethod() const {
    return method_;
}

std::string HttpRequest::GetVersion() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.find(key) != post_.end()) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != "");
    if(post_.find(key) != post_.end()) {
        return post_.find(key)->second;
    }
    return "";
}

#endif