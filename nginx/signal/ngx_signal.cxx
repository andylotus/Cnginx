  
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>     
#include <errno.h>      
#include <sys/wait.h>   

#include "ngx_global.h"
#include "ngx_macro.h"
#include "ngx_func.h" 

 //for signal
typedef struct 
{
    int           signo;        
    const  char   *signame;     

   //signal process function, provided by ourselves, but its parameter and retur nvalue is specified by OS  
    void  (*handler)(int signo, siginfo_t *siginfo, void *ucontext);  
} ngx_signal_t;

//declare a signal process function -- signal handler 
static void ngx_signal_handler(int signo, siginfo_t *siginfo, void *ucontext);  
static void ngx_process_get_status(void);                                       

 
 //define different signals in our system -- just a part of nginx
ngx_signal_t  signals[] = {
     
    { SIGHUP,    "SIGHUP",           ngx_signal_handler },         
    { SIGINT,    "SIGINT",           ngx_signal_handler },         
	{ SIGTERM,   "SIGTERM",          ngx_signal_handler },         
    { SIGCHLD,   "SIGCHLD",          ngx_signal_handler },         
    { SIGQUIT,   "SIGQUIT",          ngx_signal_handler },         
    { SIGIO,     "SIGIO",            ngx_signal_handler },         
    { SIGSYS,    "SIGSYS, SIG_IGN",  NULL               }, //set handler here as NULL, so we ask system to ignore this signal, instead of killing it        
                                                                       
    { 0,         NULL,               NULL               }   //0 here as a special mark       
};

 //initialization of signal, to register signal handler(process) function
 //return value: 0 if success, -1 if failed
int ngx_init_signals()
{
    ngx_signal_t      *sig;   
    struct sigaction   sa;    

    for (sig = signals; sig->signo != 0; sig++)   
    {                
        memset(&sa,0,sizeof(struct sigaction));

        if (sig->handler)   
        {
            sa.sa_sigaction = sig->handler;   
            sa.sa_flags = SA_SIGINFO;                                                  
        }
        else
        {
            sa.sa_handler = SIG_IGN;                                  
        }  

        sigemptyset(&sa.sa_mask);    
                                     
        //set up signal process function
        if (sigaction(sig->signo, &sa, NULL) == -1)                                               
        {   
            ngx_log_error_core(NGX_LOG_EMERG,errno,"sigaction(%s) failed",sig->signame);  
            return -1;  
        }	
        else
        {            
      
        }
    }  
    return 0;  
}

 
 //signal process function
static void ngx_signal_handler(int signo, siginfo_t *siginfo, void *ucontext)
{    
     
    ngx_signal_t    *sig;     //self-defined struct
    char            *action;  
    
    for (sig = signals; sig->signo != 0; sig++)  
    {         
        //find corresponding signal 
        if (sig->signo == signo) 
        { 
            break;
        }
    }  

    action = (char *)"";   

    if(ngx_process == NGX_PROCESS_MASTER)     //master process  
    {
        //master process run here 
        switch (signo)
        {
        case SIGCHLD:   //when child process exists, master process will receive this signal
            ngx_reap = 1;   //mark the status change for child process
            break;

        default:
            break;
        }  
    }
    else if(ngx_process == NGX_PROCESS_WORKER)  //work process
    {
         
            
    }
    else
    {
         
         
    }  

     
     //si_pid: sending process ID (the process id that sent out this signal)
    if(siginfo && siginfo->si_pid)   
    {
        ngx_log_error_core(NGX_LOG_NOTICE,0,"signal %d (%s) received from %P%s", signo, sig->signame, siginfo->si_pid, action); 
    }
    else
    {
        ngx_log_error_core(NGX_LOG_NOTICE,0,"signal %d (%s) received %s",signo, sig->signame, action); 
    }


    //usually SIGCHLD signal considered as 'accident' 
    if (signo == SIGCHLD) 
    {
        ngx_process_get_status();  //get the child process's closing status
    }  

    return;
}

//get child process' closing status, to avoid child process becomes zombie process when 'kill' child process alone 
static void ngx_process_get_status(void)
{
    pid_t            pid;
    int              status;
    int              err;
    int              one=0;  

     //when 'kill' a child process, master process will receive this SIGCHLD signal
    for ( ;; ) 
    {
        pid = waitpid(-1, &status, WNOHANG);  
                                               
        if(pid == 0)  
        {
            return;
        }  
         
        if(pid == -1) //waitpid() has error
        {
             
            err = errno;
            if(err == EINTR)            
            {
                continue;
            }

            if(err == ECHILD  && one)   
            {
                return;
            }

            if (err == ECHILD)          
            {
                ngx_log_error_core(NGX_LOG_INFO,err,"waitpid() failed!");
                return;
            }
            ngx_log_error_core(NGX_LOG_ALERT,err,"waitpid() failed!");
            return;
        }   
         
         //up to here, it means succesfully 'return process id'
        one = 1;   
        if(WTERMSIG(status))   
        {
            ngx_log_error_core(NGX_LOG_ALERT,0,"pid = %P exited on signal %d!",pid,WTERMSIG(status));  
        }
        else
        {
            ngx_log_error_core(NGX_LOG_NOTICE,0,"pid = %P exited with code %d!",pid,WEXITSTATUS(status));  
        }
    }  
    return;
}
