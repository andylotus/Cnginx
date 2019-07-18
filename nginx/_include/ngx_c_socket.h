
#ifndef __NGX_SOCKET_H__
#define __NGX_SOCKET_H__

#include <vector>        
#include <list>          
#include <sys/epoll.h>   
#include <sys/socket.h>
#include <pthread.h>     
#include <semaphore.h>   
#include <atomic>        

#include "ngx_comm.h"

 
#define NGX_LISTEN_BACKLOG  511     //completed-connection list
#define NGX_MAX_EVENTS      512     //maximum events received in epoll_wait

typedef struct ngx_listening_s   ngx_listening_t, *lpngx_listening_t;
typedef struct ngx_connection_s  ngx_connection_t,*lpngx_connection_t;
typedef class  CSocekt           CSocekt;

typedef void (CSocekt::*ngx_event_handler_pt)(lpngx_connection_t c);  

 
//for listening
struct ngx_listening_s   
{
	int                       port;       //listening port  
	int                       fd;           
	lpngx_connection_t        connection;   
};

//a tcp connection(client starts, Nginx server accept it passively)  
struct ngx_connection_s
{		
	ngx_connection_s();                                       
	virtual ~ngx_connection_s();                              
	void GetOneToUse();                                       
	void PutOneToFree();                                      


	int                       fd;                             
	lpngx_listening_t         listening;                      
 
	 
	uint64_t                  iCurrsequence;                  
	struct sockaddr           s_sockaddr;      //to store peer address               
	 

	//event_handler function, like recall-funct
	ngx_event_handler_pt      rhandler;                        
	ngx_event_handler_pt      whandler;                        

	 
	uint32_t                  events;                          
	
	//receiving pack 
	unsigned char             curStat;                         
	char                      dataHeadInfo[_DATA_BUFSIZE_];    //to save pack header info
	char                      *precvbuf;                       
	unsigned int              irecvlen;                        
	char                      *precvMemPointer;  //address of the 'new' memory to receive pack               

	pthread_mutex_t           logicPorcMutex;                  

	 
	std::atomic<int>          iThrowsendCount;                 
	char                      *psendMemPointer;                
	char                      *psendbuf;                       
	unsigned int              isendlen;                        

	 
	time_t                    inRecyTime;                      

	 
	lpngx_connection_t        next;          //equal to 'Netxt' in linked list                  
};

 //MSG header, also for pack receiving
typedef struct _STRUC_MSG_HEADER
{
	lpngx_connection_t pConn;          
	uint64_t           iCurrsequence;  
	 
}STRUC_MSG_HEADER,*LPSTRUC_MSG_HEADER;

 
class CSocekt
{
public:
	CSocekt();                                                             
	virtual ~CSocekt();                                                    
	virtual bool Initialize();                                             
	virtual bool Initialize_subproc();                                     
	virtual void Shutdown_subproc();                                       

public:
	virtual void threadRecvProcFunc(char *pMsgBuf);                        
public:	
	int  ngx_epoll_init();                                                 
	 
	                                                                       
	int  ngx_epoll_process_events(int timer);                              

	int ngx_epoll_oper_event(int fd,uint32_t eventtype,uint32_t flag,int bcaction,lpngx_connection_t pConn); 
	                                                                       
	
protected:
	 
	void msgSend(char *psendbuf);                                          

private:	
	void ReadConf();                                                       
	bool ngx_open_listening_sockets();                                     
	void ngx_close_listening_sockets();                                    
	bool setnonblocking(int sockfd);                                       

	 
	void ngx_event_accept(lpngx_connection_t oldc);                        
	void ngx_read_request_handler(lpngx_connection_t pConn);               
	void ngx_write_request_handler(lpngx_connection_t pConn);              
	void ngx_close_connection(lpngx_connection_t pConn);                   

	ssize_t recvproc(lpngx_connection_t pConn,char *buff,ssize_t buflen);  
	void ngx_wait_request_handler_proc_p1(lpngx_connection_t pConn);       
	void ngx_wait_request_handler_proc_plast(lpngx_connection_t pConn);    
	void clearMsgSendQueue();                                              

	ssize_t sendproc(lpngx_connection_t c,char *buff,ssize_t size);        

	 
	size_t ngx_sock_ntop(struct sockaddr *sa,int port,u_char *text,size_t len);   

	 
	void initconnection();                                                 
	void clearconnection();                                                
	lpngx_connection_t ngx_get_connection(int isock);                      
	void ngx_free_connection(lpngx_connection_t pConn);                    
	void inRecyConnectQueue(lpngx_connection_t pConn);                     

	 
	static void* ServerSendQueueThread(void *threadData);                  
	static void* ServerRecyConnectionThread(void *threadData);             
	
protected:
	 
	size_t                         m_iLenPkgHeader;                        
	size_t                         m_iLenMsgHeader;                        
	
private:
	struct ThreadItem   
    {
        pthread_t   _Handle;                                               
        CSocekt     *_pThis;                                               
        bool        ifrunning;                                             

         
        ThreadItem(CSocekt *pthis):_pThis(pthis),ifrunning(false){}                             
         
        ~ThreadItem(){}        
    };


	int                            m_worker_connections;                   
	int                            m_ListenPortCount;         //the number of ports listened             
	int                            m_epollhandle;                          

	 
	std::list<lpngx_connection_t>  m_connectionList;                       
	std::list<lpngx_connection_t>  m_freeconnectionList;                   
	std::atomic<int>               m_total_connection_n;                   
	std::atomic<int>               m_free_connection_n;                    
	pthread_mutex_t                m_connectionMutex;                      
	pthread_mutex_t                m_recyconnqueueMutex;                   
	std::list<lpngx_connection_t>  m_recyconnectionList;                   
	std::atomic<int>               m_totol_recyconnection_n;               
	int                            m_RecyConnectionWaitTime;               


	 
	                                                                         
	
	
	
	std::vector<lpngx_listening_t> m_ListenSocketList;         //listening socket list            
	struct epoll_event             m_events[NGX_MAX_EVENTS];               

	 
	std::list<char *>              m_MsgSendQueue;                         
	std::atomic<int>               m_iSendMsgQueueCount;                   
	 
	std::vector<ThreadItem *>      m_threadVector;                         
	pthread_mutex_t                m_sendMessageQueueMutex;                
	sem_t                          m_semEventSendQueue;                    
	
};

#endif
