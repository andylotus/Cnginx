// ip address related
 
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

 
 //change the address in the socket into txt format
 //use the parameter 1 (sa), get the string of address and port, return the string's length
size_t CSocekt::ngx_sock_ntop(struct sockaddr *sa,int port,u_char *text,size_t len)
{
    struct sockaddr_in   *sin;
    u_char               *p;

    switch (sa->sa_family)
    {
    case AF_INET:
        sin = (struct sockaddr_in *) sa;
        p = (u_char *) &sin->sin_addr;
        if(port)   
        {
            p = ngx_snprintf(text, len, "%ud.%ud.%ud.%ud:%d",p[0], p[1], p[2], p[3], ntohs(sin->sin_port));  
        }
        else  
        {
            p = ngx_snprintf(text, len, "%ud.%ud.%ud.%ud",p[0], p[1], p[2], p[3]);            
        }
        return (p - text);
        break;
    default:
        return 0;
        break;
    }
    return 0;
}
