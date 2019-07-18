//network and logic processing
  
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
 
#include "ngx_c_memory.h"
#include "ngx_c_crc32.h"
#include "ngx_c_slogic.h"  
#include "ngx_logiccomm.h"  
#include "ngx_c_lockmutex.h"  

 
typedef bool (CLogicSocket::*handler)(  lpngx_connection_t pConn,       
                                        LPSTRUC_MSG_HEADER pMsgHeader,   
                                        char *pPkgBody,                  
                                        unsigned short iBodyLength);     

 //array to save member func pointer
static const handler statusHandler[] = 
{
     
    NULL,                                                    
    NULL,                                                    
    NULL,                                                    
    NULL,                                                    
    NULL,                                                    

     
    &CLogicSocket::_HandleRegister,                          
    &CLogicSocket::_HandleLogIn,                             
     


};
#define AUTH_TOTAL_COMMANDS sizeof(statusHandler)/sizeof(handler)  

 
CLogicSocket::CLogicSocket()
{

}
 
CLogicSocket::~CLogicSocket()
{

}

 
 //initialization, before fork() child processes
bool CLogicSocket::Initialize()
{
    bool bParentInit = CSocekt::Initialize();   
    return bParentInit;
}

 
 //process data pack received
void CLogicSocket::threadRecvProcFunc(char *pMsgBuf)
{          
    LPSTRUC_MSG_HEADER pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgBuf;                   
    LPCOMM_PKG_HEADER  pPkgHeader = (LPCOMM_PKG_HEADER)(pMsgBuf+m_iLenMsgHeader);  
    void  *pPkgBody;                                                               
    unsigned short pkglen = ntohs(pPkgHeader->pkgLen);                             

    if(m_iLenPkgHeader == pkglen)
    {
         
		if(pPkgHeader->crc32 != 0)  
		{
			return;  
		}
		pPkgBody = NULL;
    }
    else 
	{
         
		pPkgHeader->crc32 = ntohl(pPkgHeader->crc32);		           
		pPkgBody = (void *)(pMsgBuf+m_iLenMsgHeader+m_iLenPkgHeader);  

		 
		int calccrc = CCRC32::GetInstance()->Get_CRC((unsigned char *)pPkgBody,pkglen-m_iLenPkgHeader);  
		if(calccrc != pPkgHeader->crc32)  
		{
            ngx_log_stderr(0,"CLogicSocket::threadRecvProcFunc()中CRC错误，丢弃数据!");     
			return;  
		}
	}

     
    unsigned short imsgCode = ntohs(pPkgHeader->msgCode);  
    lpngx_connection_t p_Conn = pMsgHeader->pConn;         

     
     
    if(p_Conn->iCurrsequence != pMsgHeader->iCurrsequence)    
    {
        return;  
    }

     
	if(imsgCode >= AUTH_TOTAL_COMMANDS)  
    {
        ngx_log_stderr(0,"CLogicSocket::threadRecvProcFunc()中imsgCode=%d消息码不对!",imsgCode);  
        return;  
    }

     
     
    if(statusHandler[imsgCode] == NULL)  
    {
        ngx_log_stderr(0,"CLogicSocket::threadRecvProcFunc()中imsgCode=%d消息码找不到对应的处理函数!",imsgCode);  
        return;   
    }

     
     
    (this->*statusHandler[imsgCode])(p_Conn,pMsgHeader,(char *)pPkgBody,pkglen-m_iLenPkgHeader);
    return;	
}

 
 //handle business logic
bool CLogicSocket::_HandleRegister(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char *pPkgBody,unsigned short iBodyLength)
{
     
    if(pPkgBody == NULL)  
    {        
        return false;
    }
		    
    int iRecvLen = sizeof(STRUCT_REGISTER); 
    if(iRecvLen != iBodyLength)  
    {     
        return false; 
    }

    
    CLock lock(&pConn->logicPorcMutex);  
    
     
    LPSTRUCT_REGISTER p_RecvInfo = (LPSTRUCT_REGISTER)pPkgBody; 


	LPCOMM_PKG_HEADER pPkgHeader;	
	CMemory  *p_memory = CMemory::GetInstance();
	CCRC32   *p_crc32 = CCRC32::GetInstance();
    int iSendLen = sizeof(STRUCT_REGISTER);  
     

iSendLen = 65000;  
    char *p_sendbuf = (char *)p_memory->AllocMemory(m_iLenMsgHeader+m_iLenPkgHeader+iSendLen,false); 
     
    memcpy(p_sendbuf,pMsgHeader,m_iLenMsgHeader);                    
     
    pPkgHeader = (LPCOMM_PKG_HEADER)(p_sendbuf+m_iLenMsgHeader);     
    pPkgHeader->msgCode = _CMD_REGISTER;	                         
    pPkgHeader->msgCode = htons(pPkgHeader->msgCode);	             
    pPkgHeader->pkgLen  = htons(m_iLenPkgHeader + iSendLen);         
     
    LPSTRUCT_REGISTER p_sendInfo = (LPSTRUCT_REGISTER)(p_sendbuf+m_iLenMsgHeader+m_iLenPkgHeader);	 
     
    
     
    pPkgHeader->crc32   = p_crc32->Get_CRC((unsigned char *)p_sendInfo,iSendLen);
    pPkgHeader->crc32   = htonl(pPkgHeader->crc32);		

     
    msgSend(p_sendbuf);
     

    return true;
}
bool CLogicSocket::_HandleLogIn(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char *pPkgBody,unsigned short iBodyLength)
{
    ngx_log_stderr(0,"执行了CLogicSocket::_HandleLogIn()!");
    return true;
}
