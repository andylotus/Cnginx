//pack/data receiving and send
 
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
#include <pthread.h>    

#include "ngx_c_conf.h"
#include "ngx_macro.h"
#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_socket.h"
#include "ngx_c_memory.h"
#include "ngx_c_lockmutex.h"   

 //read event handler
void CSocekt::ngx_read_request_handler(lpngx_connection_t pConn)
{  
    //receive pack 
    ssize_t reco = recvproc(pConn,pConn->precvbuf,pConn->irecvlen); 
    if(reco <= 0)  
    {
        return; 
    }

    if(pConn->curStat == _PKG_HD_INIT)  
    {        
        if(reco == m_iLenPkgHeader) 
        {   
            //after pack header is received
            ngx_wait_request_handler_proc_p1(pConn);  
        }
        else
		{
			//irecvlen has a default value which is m_iLenPkgHeader
            //so at most get the full header, not more than that
            pConn->curStat        = _PKG_HD_RECVING;                  
            pConn->precvbuf       = pConn->precvbuf + reco;               
            pConn->irecvlen       = pConn->irecvlen - reco;               
        }  
    } 
    else if(pConn->curStat == _PKG_HD_RECVING)  
    {
        if(pConn->irecvlen == reco)  
        { 
            ngx_wait_request_handler_proc_p1(pConn);  
        }
        else
		{ 
            pConn->precvbuf       = pConn->precvbuf + reco;               
            pConn->irecvlen       = pConn->irecvlen - reco;               
        }
    }
    else if(pConn->curStat == _PKG_BD_INIT) 
    {
         
        if(reco == pConn->irecvlen)
        {
            //after whole pack is received 
            ngx_wait_request_handler_proc_plast(pConn);
        }
        else
		{ 
			pConn->curStat = _PKG_BD_RECVING;					
			pConn->precvbuf = pConn->precvbuf + reco;
			pConn->irecvlen = pConn->irecvlen - reco;
		}
    }
    else if(pConn->curStat == _PKG_BD_RECVING) 
    {
        if(pConn->irecvlen == reco)
        { 
            ngx_wait_request_handler_proc_plast(pConn);
        }
        else
        { 
            pConn->precvbuf = pConn->precvbuf + reco;
			pConn->irecvlen = pConn->irecvlen - reco;
        }
    }   
    return;
}

 
 //receive data 
ssize_t CSocekt::recvproc(lpngx_connection_t c,char *buff,ssize_t buflen)   
{
    ssize_t n;
    
    n = recv(c->fd, buff, buflen, 0);  
    if(n == 0) //client close normally
    {
        if(close(c->fd) == -1)
        {
            ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::recvproc()中close(%d)失败!",c->fd);  
        }
        inRecyConnectQueue(c);
        return -1;
    }
     
    if(n < 0)  
    {
         
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        { 
            ngx_log_stderr(errno,"CSocekt::recvproc()中errno == EAGAIN || errno == EWOULDBLOCK成立，出乎我意料！"); 
            return -1;  
        }
        if(errno == EINTR)   
        {
             
            ngx_log_stderr(errno,"CSocekt::recvproc()中errno == EINTR成立，出乎我意料！"); 
            return -1;  
        }
        //the below is considered as exception/unormal
        if(errno == ECONNRESET)   
        {
                
        }
        else
        {
             
            ngx_log_stderr(errno,"CSocekt::recvproc()中发生错误，我打印出来看看是啥错误！");   
        } 
        
        if(close(c->fd) == -1)
        {
            ngx_log_error_core(NGX_LOG_ALERT,errno,"CSocekt::recvproc()中close_2(%d)失败!",c->fd);  
        }

        inRecyConnectQueue(c);
        return -1;
    }

   //normal read  
    return n;  
}


 //handle pack header
void CSocekt::ngx_wait_request_handler_proc_p1(lpngx_connection_t pConn)
{
    CMemory *p_memory = CMemory::GetInstance();		

    LPCOMM_PKG_HEADER pPkgHeader;
    pPkgHeader = (LPCOMM_PKG_HEADER)pConn->dataHeadInfo;  

    unsigned short e_pkgLen; 
    e_pkgLen = ntohs(pPkgHeader->pkgLen);   

    //evil or error pack
    if(e_pkgLen < m_iLenPkgHeader) 
    {
        pConn->curStat = _PKG_HD_INIT;      
        pConn->precvbuf = pConn->dataHeadInfo;
        pConn->irecvlen = m_iLenPkgHeader;
    }
    else if(e_pkgLen > (_PKG_MAX_LENGTH-1000))    
    {
        pConn->curStat = _PKG_HD_INIT;
        pConn->precvbuf = pConn->dataHeadInfo;
        pConn->irecvlen = m_iLenPkgHeader;
    }
    else
    {   //legal header data
        char *pTmpBuffer  = (char *)p_memory->AllocMemory(m_iLenMsgHeader + e_pkgLen,false);  
         
        pConn->precvMemPointer = pTmpBuffer;   

        //a) msg header
        LPSTRUC_MSG_HEADER ptmpMsgHeader = (LPSTRUC_MSG_HEADER)pTmpBuffer;
        ptmpMsgHeader->pConn = pConn;
        ptmpMsgHeader->iCurrsequence = pConn->iCurrsequence;  
        //b) pack header 
        pTmpBuffer += m_iLenMsgHeader;                  
        memcpy(pTmpBuffer,pPkgHeader,m_iLenPkgHeader);  //copy pack header data
        if(e_pkgLen == m_iLenPkgHeader)
        {
             //if a pack only has header, no pack body --- still considered as received the whole pack
            ngx_wait_request_handler_proc_plast(pConn);
        } 
        else
        {
             //begin receive pack body
            pConn->curStat = _PKG_BD_INIT;                    
            pConn->precvbuf = pTmpBuffer + m_iLenPkgHeader;   
            pConn->irecvlen = e_pkgLen - m_iLenPkgHeader;     
        }                       
    }   

    return;
}

 //after receive the whole pack
void CSocekt::ngx_wait_request_handler_proc_plast(lpngx_connection_t pConn)
{
 
    g_threadpool.inMsgRecvQueueAndSignal(pConn->precvMemPointer);  
  
    pConn->precvMemPointer = NULL;
    pConn->curStat         = _PKG_HD_INIT;      
    pConn->precvbuf        = pConn->dataHeadInfo;   
    pConn->irecvlen        = m_iLenPkgHeader;   
    return;
}


ssize_t CSocekt::sendproc(lpngx_connection_t c,char *buff,ssize_t size)   
{
     
    ssize_t   n;

    for ( ;; )
    {
        n = send(c->fd, buff, size, 0);  
        if(n > 0)  
        {        
             
             
             
             
            return n;  
        }

        if(n == 0)
        {
             
             
             
             
            return 0;
        }

        if(errno == EAGAIN)   
        {
             
            return -1;   
        }

        if(errno == EINTR) 
        {
             
             
            ngx_log_stderr(errno,"CSocekt::sendproc()中send()失败.");   
             
        }
        else
        {
             
            return -2;    
        }
    }  
}

 
 
void CSocekt::ngx_write_request_handler(lpngx_connection_t pConn)
{      
    CMemory *p_memory = CMemory::GetInstance();
    
     
    ssize_t sendsize = sendproc(pConn,pConn->psendbuf,pConn->isendlen);

    if(sendsize > 0 && sendsize != pConn->isendlen)
    {        
         
        pConn->psendbuf = pConn->psendbuf + sendsize;
		pConn->isendlen = pConn->isendlen - sendsize;	
        return;
    }
    else if(sendsize == -1)
    {
         
        ngx_log_stderr(errno,"CSocekt::ngx_write_request_handler()时if(sendsize == -1)成立，这很怪异。");  
        return;
    }

    if(sendsize > 0 && sendsize == pConn->isendlen)  
    {
         
        if(ngx_epoll_oper_event(
                pConn->fd,           
                EPOLL_CTL_MOD,       
                EPOLLOUT,            
                1,                   
                pConn                
                ) == -1)
        {
             
            ngx_log_stderr(errno,"CSocekt::ngx_write_request_handler()中ngx_epoll_oper_event()失败。");
        }    

        ngx_log_stderr(0,"CSocekt::ngx_write_request_handler()中数据发送完毕，很好。");  
        
    }

     

     
    if(sem_post(&m_semEventSendQueue)==-1)       
        ngx_log_stderr(0,"CSocekt::ngx_write_request_handler()中sem_post(&m_semEventSendQueue)失败.");


    p_memory->FreeMemory(pConn->psendMemPointer);   
    pConn->psendMemPointer = NULL;        
    --pConn->iThrowsendCount;   
    return;
}

 
 
 
void CSocekt::threadRecvProcFunc(char *pMsgBuf)
{   
    return;
}


