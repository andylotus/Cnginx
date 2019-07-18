//daemon process related 
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>      
#include <sys/stat.h>
#include <fcntl.h>

#include "ngx_func.h"
#include "ngx_macro.h"
#include "ngx_c_conf.h"

//daemon process initialization 
//return value: failed -1; sucess - child process returns 0, master process returns 1
int ngx_daemon()
{  
    switch (fork())   
    {
    case -1:
         
        ngx_log_error_core(NGX_LOG_EMERG,errno, "ngx_daemon()中fork()失败!");
        return -1;
    case 0:
        //child process 
        break;
    default:
        //parent/master process 
        return 1;   
    }  

     //only child process forked above can come here
    ngx_parent = ngx_pid;      
    ngx_pid = getpid();        
    
     //detached from terminal, terminal closed
    if (setsid() == -1)  
    {
        ngx_log_error_core(NGX_LOG_EMERG, errno,"ngx_daemon()中setsid()失败!");
        return -1;
    }

    //not let it have limits on file acess 
    umask(0); 

    int fd = open("/dev/null", O_RDWR);
    if (fd == -1) 
    {
        ngx_log_error_core(NGX_LOG_EMERG,errno,"ngx_daemon()中open(\"/dev/null\")失败!");        
        return -1;
    }
    if (dup2(fd, STDIN_FILENO) == -1)  
    {
        ngx_log_error_core(NGX_LOG_EMERG,errno,"ngx_daemon()中dup2(STDIN)失败!");        
        return -1;
    }
    if (dup2(fd, STDOUT_FILENO) == -1)  
    {
        ngx_log_error_core(NGX_LOG_EMERG,errno,"ngx_daemon()中dup2(STDOUT)失败!");
        return -1;
    }
    if (fd > STDERR_FILENO)   
     {
        if (close(fd) == -1)   
        {
            ngx_log_error_core(NGX_LOG_EMERG,errno, "ngx_daemon()中close(fd)失败!");
            return -1;
        }
    }
    return 0;  
}

