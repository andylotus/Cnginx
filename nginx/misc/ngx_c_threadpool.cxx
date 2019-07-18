//threadpool
 
#include <stdarg.h>
#include <unistd.h>   

#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_threadpool.h"
#include "ngx_c_memory.h"
#include "ngx_macro.h"

 
pthread_mutex_t CThreadPool::m_pthreadMutex = PTHREAD_MUTEX_INITIALIZER;   
pthread_cond_t CThreadPool::m_pthreadCond = PTHREAD_COND_INITIALIZER;      
bool CThreadPool::m_shutdown = false;     

CThreadPool::CThreadPool()
{
    m_iRunningThreadNum = 0;   
    m_iLastEmgTime = 0;        
     
    m_iRecvMsgQueueCount = 0;  
}

 
CThreadPool::~CThreadPool()
{     
    clearMsgRecvQueue();
}

 
void CThreadPool::clearMsgRecvQueue()
{
	char * sTmpMempoint;
	CMemory *p_memory = CMemory::GetInstance();
 
	while(!m_MsgRecvQueue.empty())
	{
		sTmpMempoint = m_MsgRecvQueue.front();		
		m_MsgRecvQueue.pop_front(); 
		p_memory->FreeMemory(sTmpMempoint);
	}	
}

 
 //create threads in CThreadpool
bool CThreadPool::Create(int threadNum)
{    
    ThreadItem *pNew;
    int err;

    m_iThreadNum = threadNum;  
    
    for(int i = 0; i < m_iThreadNum; ++i)
    {
        m_threadVector.push_back(pNew = new ThreadItem(this));              
        err = pthread_create(&pNew->_Handle, NULL, ThreadFunc, pNew);       
        if(err != 0)
        {
            ngx_log_stderr(err,"CThreadPool::Create()创建线程%d失败，返回的错误码为%d!",i,err);
            return false;
        }
        else
        {
             //success            
        }        
    }  

     
    std::vector<ThreadItem*>::iterator iter;
lblfor:
    for(iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++)
    {
        if( (*iter)->ifrunning == false)  //this condition makes sure every thread in the threadpool has been started
        {
             //this means some thread is not started
            usleep(100 * 1000);   
            goto lblfor;
        }
    }
    return true;
}

 
void* CThreadPool::ThreadFunc(void* threadData)
{
     //ThreadFunc is a static function, so it doesn't have this pointer
    ThreadItem *pThread = static_cast<ThreadItem*>(threadData);
    CThreadPool *pThreadPoolObj = pThread->_pThis;
    
    CMemory *p_memory = CMemory::GetInstance();	    
    int err;

    pthread_t tid = pthread_self();  
    while(true)
    {
        err = pthread_mutex_lock(&m_pthreadMutex);  
        if(err != 0) ngx_log_stderr(err,"CThreadPool::ThreadFunc()中pthread_mutex_lock()失败，返回的错误码为%d!",err); 
        //while works together with wait() -- to avoid spurious wakeup
        while ( (pThreadPoolObj->m_MsgRecvQueue.size() == 0) && m_shutdown == false)
        {
   
            if(pThread->ifrunning == false)            
                pThread->ifrunning = true;  
            //wait here when first start the worker process
            pthread_cond_wait(&m_pthreadCond, &m_pthreadMutex);      
        }

        if(m_shutdown)
        {   
            pthread_mutex_unlock(&m_pthreadMutex);  
            break;                     
        }

         
        char *jobbuf = pThreadPoolObj->m_MsgRecvQueue.front();      
        pThreadPoolObj->m_MsgRecvQueue.pop_front();                 
        --pThreadPoolObj->m_iRecvMsgQueueCount;                     
               
         
        err = pthread_mutex_unlock(&m_pthreadMutex); 
        if(err != 0)  ngx_log_stderr(err,"CThreadPool::ThreadFunc()中pthread_mutex_unlock()失败，返回的错误码为%d!",err); 
        
         
        ++pThreadPoolObj->m_iRunningThreadNum;     

        g_socket.threadRecvProcFunc(jobbuf);      


        p_memory->FreeMemory(jobbuf);               
        --pThreadPoolObj->m_iRunningThreadNum;      
    }  

     
    return (void*)0;
}

 //stop thread pool
void CThreadPool::StopAll() 
{
     
    if(m_shutdown == true)
    {
        return;
    }
    m_shutdown = true;

    int err = pthread_cond_broadcast(&m_pthreadCond); 
    if(err != 0)
    {         
        ngx_log_stderr(err,"CThreadPool::StopAll()中pthread_cond_broadcast()失败，返回的错误码为%d!",err);
        return;
    }

     
    std::vector<ThreadItem*>::iterator iter;
	for(iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++)
    {
        pthread_join((*iter)->_Handle, NULL);  
    }

     
    pthread_mutex_destroy(&m_pthreadMutex);
    pthread_cond_destroy(&m_pthreadCond);    

     
	for(iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++)
	{
		if(*iter)
			delete *iter;
	}
	m_threadVector.clear();

    ngx_log_stderr(0,"CThreadPool::StopAll()成功返回，线程池中线程全部正常结束!");
    return;    
}

 //this usually is place to signal/excite the free threads in the thread pool --- by Call()
void CThreadPool::inMsgRecvQueueAndSignal(char *buf)
{
     
    int err = pthread_mutex_lock(&m_pthreadMutex);     
    if(err != 0)
    {
        ngx_log_stderr(err,"CThreadPool::inMsgRecvQueueAndSignal()pthread_mutex_lock()失败，返回的错误码为%d!",err);
    }
        
    m_MsgRecvQueue.push_back(buf);	          
    ++m_iRecvMsgQueueCount;                   

  err = pthread_mutex_unlock(&m_pthreadMutex);   
    if(err != 0)
    {
        ngx_log_stderr(err,"CThreadPool::inMsgRecvQueueAndSignal()pthread_mutex_unlock()失败，返回的错误码为%d!",err);
    }
 
    Call();                                  
    return;
}

 
void CThreadPool::Call()
{ 
    int err = pthread_cond_signal(&m_pthreadCond);  
    if(err != 0 )
    {  
        ngx_log_stderr(err,"CThreadPool::Call()中pthread_cond_signal()失败，返回的错误码为%d!",err);
    }
     
    //if all threads are working/running, send out warning when time interval > 10s
    if(m_iThreadNum == m_iRunningThreadNum)  
    {        
        time_t currtime = time(NULL);
        if(currtime - m_iLastEmgTime > 10)  
        { 
            m_iLastEmgTime = currtime;   
            ngx_log_stderr(0,"CThreadPool::Call()中发现线程池中当前空闲线程数量为0，要考虑扩容线程池了!");
        }
    }  

    return;
}

 
 
