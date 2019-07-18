
#ifndef __NGX_CONF_H__
#define __NGX_CONF_H__

#include <vector>

#include "ngx_global.h"   

 
class CConfig
{
 //Singleton with correct destruction, and thread-safe -- Meyer's Singleton
private:
	CConfig();
public:
	~CConfig();
	CConfig(const CConfig&) = delete;
	CConfig& operator=(const CConfig&) = delete;

public:	
	static CConfig* GetInstance() 
	{	
		static CConfig m_instance;
		return &m_instance;
	}
 
public:
    bool Load(const char *pconfName);  
	const char *GetString(const char *p_itemname);
	int  GetIntDefault(const char *p_itemname,const int def);

public:
	std::vector<LPCConfItem> m_ConfigItemList;  //to store config info

};

#endif
