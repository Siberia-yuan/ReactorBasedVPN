#include "ParseHttp.h"
#include <exception>

namespace ParseHttp{

    ParseException::ParseException(std::string errInfo){
        m_errInfo=errInfo;
    }

    std::string ParseException::getErrInfo(){
        return m_errInfo;
    }

    void splitHeaderLine(std::vector<std::string> &headerLines,const std::string &requestContent){
        int init=0;
        for(int i=0;i<requestContent.length()-3;i++){
            if(requestContent[i]=='\r'&&requestContent[i+1]=='\n'){
                headerLines.push_back(requestContent.substr(init,i-init));
                init=i+2;
                if(requestContent[i+2]=='\r'&&'\n'){//end of header
                    return;
                }
            }else{
                continue;
            }
        }
        return;
    }

    void processSplitLines(std::vector<std::string> &headerLines,struct HeaderStruct& headerStruct){
        int l=-1;
        int r=-1;
        for(int _i=0;_i<headerLines[0].length();_i++){
            if(headerLines[0][_i]==' '){
                if(l==-1){
                    l=_i;
                    continue;
                }else{
                    r=_i;
                    headerStruct.requestUrl=headerLines[0].substr(l+1,r-l-1);
                    break;
                }
            }
        }
        for(int i=1;i<headerLines.size();i++){
            //std::cout<<"Header Lines:"<<headerLines[i]<<std::endl;
            if(headerLines[i].substr(0,5)=="Host:"){
                headerStruct.host=headerLines[i].substr(6,headerLines[i].length()-6);
            }else if(headerLines[i].substr(0,11)=="Connection:"){
                headerStruct.connection=headerLines[i].substr(12,headerLines[i].length()-12);
            }else if(headerLines[i].substr(0,17)=="Proxy-Connection:"){
                headerStruct.connection=headerLines[i].substr(18,headerLines[i].length()-18);
            }else{
                continue;
            }
        }
        return;
    }

    void getAddrnPort(const std::string &hostLine,std::string& addr,int &port){
        for(int i=0;i<hostLine.length();i++){
            if(hostLine[i]==':'){
                addr=hostLine.substr(0,i);
                port=stoi(hostLine.substr(i+1,hostLine.length()-i-1));
                return;
            }
        }
        addr=hostLine;
        port=80;//default HTTP port
        return;
    }

    void parseHttpRequest(struct HeaderStruct& headerStruct,std::string requestContent){
        if(requestContent.length()<20){
            ParseException except("Invalid Http Request:Request Content too Short!");
            throw except;
        }
        if(requestContent.substr(0,3)=="GET"){
            headerStruct.method=GET;
        }else if(requestContent.substr(0,3)=="PUT"){
            headerStruct.method=PUT;
        }else if(requestContent.substr(0,7)=="CONNECT"){
            headerStruct.method=CONNECT;
        }else if(requestContent.substr(0,4)=="POST"){
            headerStruct.method=POST;
        }else if(requestContent.substr(0,4)=="HEAD"){
            headerStruct.method=HEAD;
        }else{
            ParseException except("Invalid Http Request:Cant parse header");
            throw except;
        }
        std::vector<std::string> headerLines;
        splitHeaderLine(headerLines,requestContent);
        if(!headerLines.size()){
            ParseException except("Invalid Http Request:Cant split header lines");
            throw except;
        }
        processSplitLines(headerLines,headerStruct);//结构化解析完毕
        std::cout<<"-----Header Parsing-----"<<std::endl;
        std::cout<<headerStruct.requestUrl<<std::endl;
        std::cout<<headerStruct.host<<std::endl;
        std::cout<<headerStruct.connection<<std::endl;
        std::cout<<"-----End Header Lines---"<<std::endl;
    }
}