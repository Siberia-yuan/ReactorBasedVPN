#include "Reactor.h"
#include "ParseHttp.h"
#include <netdb.h>
#include <fcntl.h>

std::string msgHeader = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nServer: bfe/1.0.8.18\r\n\r\n<head><title>gqc love u 4ever</title></head><body>Message Test<br></body>";

Reactor::Reactor(std::string ip, int port){
    if (!initServerListener(ip, port)){
        return;
    }
    m_startFlg = true;
    m_acceptThread = std::make_shared<std::thread>(std::thread(&Reactor::acceptThreadLoop, this));
    for (int i = 0; i < WORKER_THREAD_NUM; i++){
        m_workerThread[i] = std::make_shared<std::thread>(std::thread(&Reactor::workerThreadLoop, this));
    }
}

Reactor::~Reactor(){
}

void Reactor::start(){
    while (m_startFlg){
        struct epoll_event evs[1024];
        memset(&evs, 0, sizeof(evs));
        int n = epoll_wait(m_epfd, evs, 1024, 20); //20ms wait once
        if (n <= 0){
            continue;
        }
        for (int i = 0; i < n; i++){
            int evFd = evs[i].data.fd;
            if (evFd == m_serverfd){
                continue;
            }else{
                //检验是否属于代理两端socket
                int caseFlag=-1;
                int valuefd=-1;
                {
                    std::unique_lock<std::mutex> lck(m_mapMtx);
                    if(m_host2Clnt.count(evFd)){
                        caseFlag=0;//is a proxy host socket send to client
                        valuefd=m_host2Clnt[evFd];//clnt
                    }else if(m_clnt2Host.count(evFd)){
                        caseFlag=1;//is a proxy clnt socket send to host
                        valuefd=m_clnt2Host[evFd];
                    }else{
                        caseFlag=-1;
                    }
                }
                {
                    std::unique_lock<std::mutex> lck(mtx);
                    if(caseFlag==0){
                        task_t htoc=std::bind(&Reactor::sendDataToClient,this,valuefd,evFd);
                        m_tskQueue.push_back(htoc);
                    }else if(caseFlag==1){
                        task_t ctoh=std::bind(&Reactor::sendDataToHost,this,evFd,valuefd);
                        m_tskQueue.push_back(ctoh);
                    }else{
                        task_t procFun=std::bind(&Reactor::processFd,this,evFd);
                        m_tskQueue.push_back(procFun);
                    }
                    cond.notify_one();
                }
            }
        }
    }
}

void Reactor::closeOneConnection(int fd){
    std::cout<<"_____Close Connection_____:"<<fd<<std::endl;
    close(fd);
    epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, NULL);
    return;
}

void Reactor::workerThreadLoop(){
    while (m_startFlg){
        int fd;
        task_t fun;
        {
            std::unique_lock<std::mutex> lck(mtx);
            while(!m_tskQueue.size()){
                cond.wait(lck);
            }
            fun=m_tskQueue.front();
            m_tskQueue.pop_front();
        }
        fun();
    }
}

void Reactor::processFd(int fd){
    char buffer[1024];
    int readLength;
    std::string clntMsg = "";
    while (1){
        memset(&buffer, 0, sizeof(buffer));
        readLength = read(fd, &buffer, 1024);
        //char *readPoint=buffer;
        if(errno==EWOULDBLOCK){
            continue;
        }
        if (readLength == -1){ //对端关闭or重置链接
            break;
        }
        std::string s(buffer, buffer + readLength);
        clntMsg += s;
        //std::cout<<s<<std::endl;
        if (readLength < 1024){
            break;
        }
    }
    ParseHttp::HeaderStruct headerStruct;
    try{
        ParseHttp::parseHttpRequest(headerStruct, clntMsg);
    }catch (ParseHttp::ParseException e){
        std::cout << e.getErrInfo() << std::endl;
        closeOneConnection(fd);
        return;
    }
    if(headerStruct.host.substr(0,12)!=LOCALIP){
        int port=80;
        std::string addr="";
        ParseHttp::getAddrnPort(headerStruct.host,addr,port);
        // std::cout<<"Addr:"<<addr<<std::endl;
        // std::cout<<"Port:"<<port<<std::endl;
        std::unique_lock<std::mutex> lck(mtx);//acquire lock and add connection task;
        task_t initRemoteFun=std::bind(&Reactor::initRemoteSend,this,addr,port,clntMsg,fd);
        m_tskQueue.push_back(initRemoteFun);
        cond.notify_one();
        if(headerStruct.connection=="keep-alive"||headerStruct.connection=="Keep-Alive"){
            {
                std::unique_lock<std::mutex> lck(m_kpAliveMtx);
                m_fdKeepAlive.insert(fd);
                m_kpAliveCond.notify_one();
            }
        }
    }else{
        write(fd, msgHeader.c_str(), msgHeader.length());
        // std::cout << "---------Client Message----------" << std::endl;
        // std::cout << clntMsg << std::endl;
        // std::cout << "--------------End----------------\n\n" << std::endl;
        closeOneConnection(fd);
    }
}

void Reactor::initRemoteSend(std::string addr,int port,std::string Msg,int clntFd){
    std::cout<<"------Init Remote Connection------"<<std::endl;
    int fd=socket(AF_INET,SOCK_STREAM, 0);//作为客户端阻塞的socket
    struct sockaddr_in remoteServerAddr;
    struct hostent *hptr;
    if((hptr=gethostbyname(addr.c_str()))==NULL){
        std::cout<<"Fails to resolute address"<<std::endl;
        return;
    }
    char addressContent[32];
    const char *reAddr = inet_ntop(hptr->h_addrtype, hptr->h_addr,addressContent,sizeof(addressContent));
    std::cout<<"Ip is:"<<reAddr<<std::endl;
    std::cout<<"Addr is:"<<addr<<std::endl;
    std::cout<<"Port is:"<<port<<std::endl;
    memset(&remoteServerAddr,0,sizeof(remoteServerAddr));
    remoteServerAddr.sin_family = AF_INET;
    remoteServerAddr.sin_port = htons(port);
    remoteServerAddr.sin_addr.s_addr = inet_addr(reAddr);
    if(connect(fd,(sockaddr*)&remoteServerAddr,sizeof(remoteServerAddr))==-1){
        //to be continue...同时关闭远程客户端连接
        std::cout<<"Connection Fails......"<<std::endl;
        closeOneConnection(clntFd);
        return;
    }
    std::cout<<"Connection Success......"<<std::endl;
    //写入数据对
    {
        std::unique_lock<std::mutex> lck(m_mapMtx);
        m_clnt2Host[clntFd]=fd;
        m_host2Clnt[fd]=clntFd;
        m_mapCond.notify_one();
    }
    struct epoll_event ev;
    memset(&ev,0,sizeof(ev));
    ev.data.fd=fd;
    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    epoll_ctl(m_epfd,EPOLL_CTL_ADD,fd,&ev);
    std::cout<<"init connection:"<<addr<<port<<std::endl;
    if(write(fd,Msg.c_str(),Msg.length())==-1){
        //process...to be continue
        std::cout<<"Write Message Fails"<<std::endl;
        return;
    }
    std::cout<<Msg<<std::endl;
    std::cout<<"----Write Message to remote host----"<<std::endl;
}

void Reactor::acceptThreadLoop(){
    while (m_startFlg){
        struct sockaddr_in clntSock;
        memset(&clntSock, 0, sizeof(clntSock));
        socklen_t sockLen = sizeof(clntSock);
        int clntFd = accept(m_serverfd, (sockaddr *)&clntSock, &sockLen);

        int tempFlag = fcntl(clntFd, F_GETFL, 0);
        tempFlag |= O_NONBLOCK;
        fcntl(clntFd, F_SETFL, tempFlag);

        //std::cout << "Accept a connection!" << std::endl;
        struct epoll_event ev;
        ev.data.fd = clntFd;
        ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
        if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, clntFd, &ev) == -1){
            std::cout << "Cant listen" << std::endl;
        }
    }
}

bool Reactor::initServerListener(std::string ip, int port){
    m_serverfd = socket(AF_INET, SOCK_STREAM, 0);

    int val=1;
    setsockopt(m_serverfd,SOL_SOCKET,SO_REUSEPORT,&val,sizeof(val));

    if (m_serverfd == -1){
        std::cout << "Fails to init server socket" << std::endl;
        return false;
    }
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
    if (bind(m_serverfd, (sockaddr *)&serverAddr, sizeof(serverAddr)) == -1){
        std::cout << "Bind Fails" << std::endl;
        return false;
    }
    if (listen(m_serverfd, 10) == -1){
        std::cout << "Listen Fails" << std::endl;
        return false;
    }

    m_epfd = epoll_create(1);
    if (m_epfd == -1){
        std::cout << "Create Epoll Fails" << std::endl;
        return false;
    }
    struct epoll_event listenEv;
    memset(&listenEv, 0, sizeof(listenEv));
    listenEv.data.fd = m_serverfd;
    listenEv.events = EPOLLIN | EPOLLRDHUP;

    if (epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_serverfd, &listenEv) == -1){
        return false;
    }
    return true;
}

//To be fixed:任务队列里产生了非常多的这个函数,而且不知道是什么原因产生的
void Reactor::sendDataToClient(int clntFd,int hostFd){
    std::string Msg="";
    char buffer[1024];
    int readLength=0;
    bool remoteClose = false;
    while(1){
        memset(&buffer,0,sizeof(buffer));
        readLength=read(hostFd,&buffer,1024);//这是一个阻塞的socket
        if(readLength==-1){
            remoteClose=true;
            break;
        }
        std::string s(buffer,buffer+readLength);
        Msg+=s;
        if(readLength<1024&&readLength>=0){
            break;
        }
    }
    if(Msg.length()){
        write(clntFd,Msg.c_str(),Msg.length());
        std::cout<<"Send to Client!"<<std::endl;
        std::cout<<Msg<<std::endl;
        std::cout<<"\n\n\n";
    }
    //服务器远程关闭连接，两端socket都可以关闭了
    if(remoteClose){
        closeOneConnection(clntFd);
        closeOneConnection(hostFd);
        {
            std::unique_lock<std::mutex> m_mapMtx;
            m_clnt2Host.erase(clntFd);
            m_host2Clnt.erase(hostFd);
        }
        //从keepalive中抹除
        {
            std::unique_lock<std::mutex> lck(m_kpAliveMtx);
            if(m_fdKeepAlive.find(clntFd)!=m_fdKeepAlive.end()){//属于长链接
                m_fdKeepAlive.erase(clntFd);
            }
        }
        std::cout<<"Close!"<<std::endl;
        return;
    }
    //查看是否属于http长链接的socket，如果不是就发送完数据关闭两端连接
    bool KeepAlive=true;
    {
        std::unique_lock<std::mutex> lck(m_kpAliveMtx);
        if(m_fdKeepAlive.find(clntFd)==m_fdKeepAlive.end()){//不属于长链接
            KeepAlive=false;
        }
    }
    if(!KeepAlive){
        closeOneConnection(clntFd);
        closeOneConnection(hostFd);
        {
            std::unique_lock<std::mutex> m_mapMtx;
            m_clnt2Host.erase(clntFd);
            m_host2Clnt.erase(hostFd);
        }
        return;
    }
}

void Reactor::sendDataToHost(int clntFd,int hostFd){
    std::string Msg="";
    char buffer[1024];
    int readLength=0;
    bool remoteClose = false;
    while(1){
        memset(&buffer,0,sizeof(buffer));
        readLength=read(clntFd,&buffer,1024);//这是一个阻塞的socket
        if(errno==EWOULDBLOCK){
            continue;
        }
        if(readLength==-1){//客户端关闭,重置连接
            remoteClose=true;
            break;
        }
        std::string s(buffer,buffer+readLength);
        Msg+=s;
        if(readLength<1024&&readLength>=0){
            break;
        }
    }
    write(hostFd,Msg.c_str(),Msg.length());
    std::cout<<"Send to Host!"<<std::endl;
    if(remoteClose){
        closeOneConnection(clntFd);
        closeOneConnection(hostFd);
        {
            std::unique_lock<std::mutex> m_mapMtx;
            m_clnt2Host.erase(clntFd);
            m_host2Clnt.erase(hostFd);
        }
        //从keepalive中抹除
        {
            std::unique_lock<std::mutex> lck(m_kpAliveMtx);
            if(m_fdKeepAlive.find(clntFd)!=m_fdKeepAlive.end()){//属于长链接
                m_fdKeepAlive.erase(clntFd);
            }
        }
        return;
    }
}