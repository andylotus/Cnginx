//network related
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>     
#include <stdarg.h>     
#include <unistd.h>     
#include <sys/time.h>   
#include <time.h>       
#include <fcntl.h>      
#include <errno.h>      
#include <sys/ioctl.h>  
#include <arpa/inet.h>

#include "ngx_c_conf.h"
#include "ngx_macro.h"
#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_socket.h"
#include "ngx_c_memory.h"
#include "ngx_c_lockmutex.h"

 
CSocekt::CSocekt()
{
    m_worker_connections = 1;       
    m_ListenPortCount = 1;          //listen 1 port
    m_RecyConnectionWaitTime = 60;  
   
    m_epollhandle = -1;           
     
    m_iLenPkgHeader = sizeof(COMM_PKG_HEADER);     
    m_iLenMsgHeader =  sizeof(STRUC_MSG_HEADER);   

    m_iSendMsgQueueCount = 0;      
    m_totol_recyconnection_n = 0;  
    return;	
}

 
 //Initialization before creating/forking child/worker process!
 bool CSocekt::Initialize()
{
    ReadConf();   
    if(ngx_open_listening_sockets() == false)   
        return false;  
    return true;
}

 
bool CSocekt::Initialize_subproc()
{
     
    if(pthread_mutex_init(&m_sendMessageQueueMutex, NULL)  != 0)
    {        
        ngx_log_stderr(0,"CSocekt::Initialize()中pthread_mutex_init(&m_sendMessageQueueMutex)失败.");
        return false;    
    }
     
    if(pthread_mutex_init(&m_connectionMutex, NULL)  != 0)
    {
        ngx_log_stderr(0,"CSocekt::Initialize()中pthread_mutex_init(&m_connectionMutex)失败.");
        return false;    
    }    
     
    if(pthread_mutex_init(&m_recyconnqueueMutex, NULL)  != 0)
    {
        ngx_log_stderr(0,"CSocekt::Initialize()中pthread_mutex_init(&m_recyconnqueueMutex)失败.");
        return false;    
    } 
   

    if(sem_init(&m_semEventSendQueue,0,0) == -1)
    {
        ngx_log_stderr(0,"CSocekt::Initialize()中sem_init(&m_semEventSendQueue,0,0)失败.");
        return false;
    }

     
    int err;
    ThreadItem *pSendQueue;     
    m_threadVector.push_back(pSendQueue = new ThreadItem(this));                          
    err = pthread_create(&pSendQueue->_Handle, NULL, ServerSendQueueThread,pSendQueue);  
    if(err != 0)
    {
        return false;
    }

     
    ThreadItem *pRecyconn;     
    m_threadVector.push_back(pRecyconn = new ThreadItem(this)); 
    err = pthread_create(&pRecyconn->_Handle, NULL, ServerRecyConnectionThread,pRecyconn);
    if(err != 0)
    {
        return false;
    }
    return true;
}

 
 
CSocekt::~CSocekt()
{ 
    std::vector<lpngx_listening_t>::iterator pos;	
	for(pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos)  
	{		
		delete (*pos);  
	} 
	m_ListenSocketList.clear();    
    return;
}

 
void CSocekt::Shutdown_subproc()
{
     
     
    if(sem_post(&m_semEventSendQueue)==-1)   
    {
         ngx_log_stderr(0,"CSocekt::Shutdown_subproc()中sem_post(&m_semEventSendQueue)失败.");
    }

    std::vector<ThreadItem*>::iterator iter;
	for(iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++)
    {
        pthread_join((*iter)->_Handle, NULL);  
    }
     
	for(iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++)
	{
		if(*iter)
			delete *iter;
	}
	m_threadVector.clear();

     
    clearMsgSendQueue();
    clearconnection();
    
     
    pthread_mutex_destroy(&m_connectionMutex);           
    pthread_mutex_destroy(&m_sendMessageQueueMutex);     
    pthread_mutex_destroy(&m_recyconnqueueMutex);        
    sem_destroy(&m_semEventSendQueue);                   
}

 
void CSocekt::clearMsgSendQueue()
{
	char * sTmpMempoint;
	CMemory *p_memory = CMemory::GetInstance();
	
	while(!m_MsgSendQueue.empty())
	{
		sTmpMempoint = m_MsgSendQueue.front();
		m_MsgSendQueue.pop_front(); 
		p_memory->FreeMemory(sTmpMempoint);
	}	
}

 
void CSocekt::ReadConf()
{
    CConfig *p_config = CConfig::GetInstance();
    m_worker_connections      = p_config->GetIntDefault("worker_connections",m_worker_connections);               
    m_ListenPortCount         = p_config->GetIntDefault("ListenPortCount",m_ListenPortCount);                     
    m_RecyConnectionWaitTime  = p_config->GetIntDefault("Sock_RecyConnectionWaitTime",m_RecyConnectionWaitTime);  
    return;
}

 
//run this function before creating worker process
 //support multiple ports 
bool CSocekt::ngx_open_listening_sockets()
{    
    int                isock;                 
    struct sockaddr_in serv_addr;             
    int                iport;                 
    char               strinfo[100];          //tmp string
   
     
    memset(&serv_addr,0,sizeof(serv_addr));   
    serv_addr.sin_family = AF_INET;                 //IPV4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  //listen all local ip addresses

     
    CConfig *p_config = CConfig::GetInstance();
    for(int i = 0; i < m_ListenPortCount; i++)  
    {        
        //create socket
        isock = socket(AF_INET,SOCK_STREAM,0);  
        if(isock == -1)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中socket()失败,i=%d.",i);
             
            return false;
        }

        //setsockopt
        int reuseaddr = 1;   
        if(setsockopt(isock,SOL_SOCKET, SO_REUSEADDR,(const void *) &reuseaddr, sizeof(reuseaddr)) == -1)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中setsockopt(SO_REUSEADDR)失败,i=%d.",i);
            close(isock);  
            return false;
        }
         //set the socket as non-block
        if(setnonblocking(isock) == false)
        {                
            ngx_log_stderr(errno,"CSocekt::Initialize()中setnonblocking()失败,i=%d.",i);
            close(isock);
            return false;
        }

         //get local port (80, 443...)
        strinfo[0] = 0;
        sprintf(strinfo,"ListenPort%d",i);
        iport = p_config->GetIntDefault(strinfo,10000);
        serv_addr.sin_port = htons((in_port_t)iport);    

         //bind (server's IP and port)
        if(bind(isock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize()中bind()失败,i=%d.",i);
            close(isock);
            return false;
        }
        
         //listen
        if(listen(isock,NGX_LISTEN_BACKLOG) == -1)
        {
            ngx_log_stderr(errno,"CSocekt::Initialize(): listen() failed,i=%d.",i);
            close(isock);
            return false;
        }

         //after listen successfully
        lpngx_listening_t p_listensocketitem = new ngx_listening_t;  
        memset(p_listensocketitem,0,sizeof(ngx_listening_t));       
        p_listensocketitem->port = iport;                           
        p_listensocketitem->fd   = isock;    
        //just for logging                       
        ngx_log_error_core(NGX_LOG_INFO,0,"listen%dport successfully!",iport);  
        m_ListenSocketList.push_back(p_listensocketitem);           
    }  
    if(m_ListenSocketList.size() <= 0)   
        return false;
    return true;
}

//set socket as non-block 
bool CSocekt::setnonblocking(int sockfd) 
{    
    int nb=1;  
    if(ioctl(sockfd, FIONBIO, &nb) == -1)  
    {
        return false;
    }
    return true;   
}

//close listening sockets 
void CSocekt::ngx_close_listening_sockets()
{
    for(int i = 0; i < m_ListenPortCount; i++)  
    {  
         
        close(m_ListenSocketList[i]->fd);
        ngx_log_error_core(NGX_LOG_INFO,0,"close listening port%d!",m_ListenSocketList[i]->port);  
    } 
    return;
}

 
void CSocekt::msgSend(char *psendbuf) 
{
    CLock lock(&m_sendMessageQueueMutex);   
    m_MsgSendQueue.push_back(psendbuf);    
    ++m_iSendMsgQueueCount;    

     
    if(sem_post(&m_semEventSendQueue)==-1)   
    {
         ngx_log_stderr(0,"CSocekt::msgSend()中sem_post(&m_semEventSendQueue)失败.");      
    }
    return;
}

 
 //epoll_create()
 //epoll intialization, done in worker process, called in ngx_worker_process_init()
int CSocekt::ngx_epoll_init()
{ 
    m_epollhandle = epoll_create(m_worker_connections);    
    if (m_epollhandle == -1) 
    {
        ngx_log_stderr(errno,"CSocekt::ngx_epoll_init()中epoll_create()失败.");
        exit(2);  
    }

     //build connection pool[i.e. vector]  --- 1024 = m_connection_n = m_worker_connections
     //? the size of pool is equal to the total number of events in epoll watch/wait
    initconnection();
     
    
    //iterate all listening socket. We will have one connection-pool element for each listening socket
    std::vector<lpngx_listening_t>::iterator pos;	
	for(pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end(); ++pos)
    {
        //get a free item/element from the connection-pool, which will be
        //used to store/bind_with a TCP connection from a client
        lpngx_connection_t p_Conn = ngx_get_connection((*pos)->fd);  
        if (p_Conn == NULL)
        {  
            ngx_log_stderr(errno,"CSocekt::ngx_epoll_init()中ngx_get_connection()失败.");
            exit(2);  
        }
        //object search/correlation: listening <---> connection
        //similar to muduo
        p_Conn->listening = (*pos);    
        (*pos)->connection = p_Conn;   

        //event_handler for read event on listening port
        p_Conn->rhandler = &CSocekt::ngx_event_accept;

         
         //add watching 'read' event to the listening socket
        if(ngx_epoll_oper_event(
                                (*pos)->fd,          
                                EPOLL_CTL_ADD,       
                                EPOLLIN|EPOLLRDHUP,  
                                0,                   
                                p_Conn               
                                ) == -1) 
        {
            exit(2);  
        }
    }  
    return 1;
}

 
//epoll_ctl()
int CSocekt::ngx_epoll_oper_event(
                        int                fd,                
                        uint32_t           eventtype,         
                        uint32_t           flag,              
                        int                bcaction,          
                        lpngx_connection_t pConn              
                        )
{
    struct epoll_event ev;    
    memset(&ev, 0, sizeof(ev));

    if(eventtype == EPOLL_CTL_ADD)  
    {

        ev.events = flag;       
        pConn->events = flag;   
    }
    else if(eventtype == EPOLL_CTL_MOD)
    {
         
        ev.events = pConn->events;   
        if(bcaction == 0)
        {
             
            ev.events |= flag;
        }
        else if(bcaction == 1)
        {
             
            ev.events &= ~flag;
        }
        else
        {
             
            ev.events = flag;       
        }
        pConn->events = ev.events;  
    }
    else
    {
         
        return  1;   
    } 

  
    ev.data.ptr = (void *)pConn;

    if(epoll_ctl(m_epollhandle,eventtype,fd,&ev) == -1)
    {
        ngx_log_stderr(errno,"CSocekt::ngx_epoll_oper_event()中epoll_ctl(%d,%ud,%ud,%d)失败.",fd,eventtype,flag,bcaction);    
        return -1;
    }
    return 1;
}

 
 //begin to get events that are happening
 int CSocekt::ngx_epoll_process_events(int timer) 
{   
     //wait events, events will be returned into m_events --- NGX_MAX_EVNETS in maximum number
    int events = epoll_wait(m_epollhandle,m_events,NGX_MAX_EVENTS,timer);
    
    if(events == -1)
    {       
        if(errno == EINTR) 
        {  
            ngx_log_error_core(NGX_LOG_INFO,errno,"CSocekt::ngx_epoll_process_events()中epoll_wait()失败!"); 
            return 1;   
        }
        else
        { 
            ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::ngx_epoll_process_events()中epoll_wait()失败!"); 
            return 0;   
        }
    }

    if(events == 0)  
    {
        if(timer != -1)
        {     
            return 1; //normal return
        }
         
        ngx_log_error_core(NGX_LOG_ALERT,0,"CSocekt::ngx_epoll_process_events()中epoll_wait()没超时却没返回任何事件!"); 
        return 0;  //unormal return
    }

 
    lpngx_connection_t p_Conn;
     
    uint32_t           revents;
    for(int i = 0; i < events; ++i)     
    {
        p_Conn = (lpngx_connection_t)(m_events[i].data.ptr);             
        revents = m_events[i].events; 

        //filter timeouted/out-dated events
        //if(c->fd == -1) 
        //if(c->instance != instance)

        if(revents & EPOLLIN)   
        {  
            (this->* (p_Conn->rhandler) )(p_Conn);     
                                              
        }
        
        if(revents & EPOLLOUT)  
        {
            //error 
            if(revents & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))  
            {
     
                --p_Conn->iThrowsendCount;                 
            }
            else
            {
                (this->* (p_Conn->whandler) )(p_Conn);    
            }
            
        }
    }  
    return 1;
}

 
 
void* CSocekt::ServerSendQueueThread(void* threadData)
{    
    ThreadItem *pThread = static_cast<ThreadItem*>(threadData);
    CSocekt *pSocketObj = pThread->_pThis;
    int err;
    std::list <char *>::iterator pos,pos2,posend;
    
    char *pMsgBuf;	
    LPSTRUC_MSG_HEADER	pMsgHeader;
	LPCOMM_PKG_HEADER   pPkgHeader;
    lpngx_connection_t  p_Conn;
    unsigned short      itmp;
    ssize_t             sendsize;  

    CMemory *p_memory = CMemory::GetInstance();
    
    while(g_stopEvent == 0)  
    {
         
         
         
        if(sem_wait(&pSocketObj->m_semEventSendQueue) == -1)
        {
             
            if(errno != EINTR)  
                ngx_log_stderr(errno,"CSocekt::ServerSendQueueThread()中sem_wait(&pSocketObj->m_semEventSendQueue)失败.");            
        }

         
        if(g_stopEvent != 0)   
            break;

        if(pSocketObj->m_iSendMsgQueueCount > 0)  
        {
            err = pthread_mutex_lock(&pSocketObj->m_sendMessageQueueMutex);  
            if(err != 0) ngx_log_stderr(err,"CSocekt::ServerSendQueueThread()中pthread_mutex_lock()失败，返回的错误码为%d!",err);

            pos    = pSocketObj->m_MsgSendQueue.begin();
			posend = pSocketObj->m_MsgSendQueue.end();

            while(pos != posend)
            {
                pMsgBuf = (*pos);                           
                pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgBuf;   
                pPkgHeader = (LPCOMM_PKG_HEADER)(pMsgBuf+pSocketObj->m_iLenMsgHeader);	 
                p_Conn = pMsgHeader->pConn;

                 
                      
                if(p_Conn->iCurrsequence != pMsgHeader->iCurrsequence) 
                {
                     
                    pos2=pos;
                    pos++;
                    pSocketObj->m_MsgSendQueue.erase(pos2);
                    --pSocketObj->m_iSendMsgQueueCount;  
                    p_memory->FreeMemory(pMsgBuf);	
                    continue;
                }  

                if(p_Conn->iThrowsendCount > 0) 
                {
                     
                    pos++;
                    continue;
                }
            
                 
                p_Conn->psendMemPointer = pMsgBuf;       
                pos2=pos;
				pos++;
                pSocketObj->m_MsgSendQueue.erase(pos2);
                --pSocketObj->m_iSendMsgQueueCount;       
                p_Conn->psendbuf = (char *)pPkgHeader;    
                itmp = ntohs(pPkgHeader->pkgLen);         
                p_Conn->isendlen = itmp;                  
                                
                 
                     
	                 
	                 
	                 
	                 
                 
                ngx_log_stderr(errno,"即将发送数据%ud。",p_Conn->isendlen);

                sendsize = pSocketObj->sendproc(p_Conn,p_Conn->psendbuf,p_Conn->isendlen);  
                if(sendsize > 0)
                {                    
                    if(sendsize == p_Conn->isendlen)  
                    {
                         
                        p_memory->FreeMemory(p_Conn->psendMemPointer);   
                        p_Conn->psendMemPointer = NULL;
                        p_Conn->iThrowsendCount = 0;   
                        ngx_log_stderr(0,"CSocekt::ServerSendQueueThread()中数据发送完毕，很好。");  
                    }
                    else   
                    {                        
                         
                        p_Conn->psendbuf = p_Conn->psendbuf + sendsize;
				        p_Conn->isendlen = p_Conn->isendlen - sendsize;	
                         
                        ++p_Conn->iThrowsendCount;              
                        if(pSocketObj->ngx_epoll_oper_event(
                                p_Conn->fd,          
                                EPOLL_CTL_MOD,       
                                EPOLLOUT,            
                                0,                   
                                p_Conn               
                                ) == -1)
                        {
                             
                            ngx_log_stderr(errno,"CSocekt::ServerSendQueueThread()ngx_epoll_oper_event()失败.");
                        }

                        ngx_log_stderr(errno,"CSocekt::ServerSendQueueThread()中数据没发送完毕【发送缓冲区满】，整个要发送%d，实际发送了%d。",p_Conn->isendlen,sendsize);

                    }  
                    continue;   
                }   

                 
                else if(sendsize == 0)
                {
                     
                     
                     
                     
                     
                    p_memory->FreeMemory(p_Conn->psendMemPointer);   
                    p_Conn->psendMemPointer = NULL;
                    p_Conn->iThrowsendCount = 0;   
                    continue;
                }

                 
                else if(sendsize == -1)
                {
                     
                    ++p_Conn->iThrowsendCount;  
                    if(pSocketObj->ngx_epoll_oper_event(
                                p_Conn->fd,          
                                EPOLL_CTL_MOD,       
                                EPOLLOUT,            
                                0,                   
                                p_Conn               
                                ) == -1)
                    {
                         
                        ngx_log_stderr(errno,"CSocekt::ServerSendQueueThread()中ngx_epoll_add_event()_2失败.");
                    }
                    continue;
                }

                else
                {
                     
                    p_memory->FreeMemory(p_Conn->psendMemPointer);   
                    p_Conn->psendMemPointer = NULL;
                    p_Conn->iThrowsendCount = 0;   
                    continue;
                }

            }  

            err = pthread_mutex_unlock(&pSocketObj->m_sendMessageQueueMutex); 
            if(err != 0)  ngx_log_stderr(err,"CSocekt::ServerSendQueueThread()pthread_mutex_unlock()失败，返回的错误码为%d!",err);
            
        }  
    }  
    
    return (void*)0;
}
