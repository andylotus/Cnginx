
#ifndef __NGX_C_SLOGIC_H__
#define __NGX_C_SLOGIC_H__

#include <sys/socket.h>
#include "ngx_c_socket.h"

 //logic and comuunication sub-class
class CLogicSocket : public CSocekt    
{
public:
	CLogicSocket();                                                          
	virtual ~CLogicSocket();                                                 
	virtual bool Initialize();                                               

public:
	 
	bool _HandleRegister(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char *pPkgBody,unsigned short iBodyLength);
	bool _HandleLogIn(lpngx_connection_t pConn,LPSTRUC_MSG_HEADER pMsgHeader,char *pPkgBody,unsigned short iBodyLength);

public:
	virtual void threadRecvProcFunc(char *pMsgBuf);
};

#endif
