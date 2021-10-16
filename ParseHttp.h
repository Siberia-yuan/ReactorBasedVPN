#ifndef _PARSE_HTTP_
#define _PARSE_HTTP_

#include<string>
#include<exception>
#include<iostream>
#include<vector>

namespace ParseHttp{

enum HttpMethod{
    GET,
    PUT,
    POST,
    CONNECT,
    HEAD
};

struct HeaderStruct{
    HttpMethod method;
    std::string requestUrl="";
    std::string host="";
    std::string connection="";
};

class ParseException:public std::exception{
    private:
        std::string m_errInfo;
    public:
        ParseException(std::string);
        std::string getErrInfo();
};

void parseHttpRequest(struct HeaderStruct& headerStruct,std::string requestContent);

void splitHeaderLine(std::vector<std::string> &headerLines,const std::string &requestContent);

void processSplitLines(std::vector<std::string> &headerLines,struct HeaderStruct& headerStruct);

void getAddrnPort(const std::string &hostLine,std::string& addr,int &port);

}

#endif