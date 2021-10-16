#ifndef SF_REACTOR
#define SF_REACTOR

#include<arpa/inet.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<unistd.h>

#include<iostream>
#include<string>
#include<cstring>
#include<mutex>
#include<condition_variable>
#include<thread>
#include<memory>
#include<functional>
#include<deque>
#include<set>
#include<unordered_map>

#define WORKER_THREAD_NUM 10
const std::string LOCALIP="43.128.4.106";//length is 12

//可以添加一个终止的内容
typedef std::function<void()> task_t;

class Reactor{
    public:
        Reactor(std::string ip,int port);
        void start();
        //void terminate();
        ~Reactor();
    private:
        //处理连接fd专用
        void processFd(int fd);
        void initRemoteSend(std::string addr,int port,std::string Msg,int clntFd);
        void sendDataToClient(int clntFd,int hostFd);
        void sendDataToHost(int clntFd,int hostFd);

        bool initServerListener(std::string ip, int port);
        void closeOneConnection(int fd);
        void workerThreadLoop();
        void acceptThreadLoop();
        std::shared_ptr<std::thread> m_workerThread[WORKER_THREAD_NUM];
        std::shared_ptr<std::thread> m_acceptThread;
        int m_epfd;
        int m_serverfd;
        bool m_startFlg;

        std::deque<task_t> m_tskQueue;
        std::mutex mtx;
        std::condition_variable cond;

        std::set<int> m_fdKeepAlive;
        std::mutex m_kpAliveMtx;
        std::condition_variable m_kpAliveCond;

        //proxy only
        std::unordered_map<int,int> m_clnt2Host;
        std::unordered_map<int,int> m_host2Clnt;
        std::mutex m_mapMtx;
        std::condition_variable m_mapCond;

        // std::deque<int> m_fdqueue;
        // std::set<int> m_queueSets;
};

#endif