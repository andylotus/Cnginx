//entrance of the framework

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h> 
#include <errno.h>
#include <arpa/inet.h>

#include "ngx_macro.h"          //for macros
#include "ngx_func.h"           //function declaration
#include "ngx_c_conf.h"         //classes to process conf file
#include "ngx_c_socket.h"       //sockets
#include "ngx_c_memory.h"       //memeory allocation and free
#include "ngx_c_threadpool.h"   //multi-thread
#include "ngx_c_crc32.h"        //crc32
#include "ngx_c_slogic.h"       //socket communication

//free resource before exit 
static void freeresource();

//for argv name setup 
size_t  g_argvneedmem=0;         
size_t  g_envneedmem=0;          
int     g_os_argc;               
char    **g_os_argv;             
char    *gp_envmem=NULL;         
int     g_daemonized=0;          

 
 
CLogicSocket   g_socket;         
CThreadPool    g_threadpool;     

 
pid_t   ngx_pid;                 
pid_t   ngx_parent;              
int     ngx_process;             
int     g_stopEvent;             

sig_atomic_t  ngx_reap;          
                                    

 
int main(int argc, char *const *argv)
{   
    int exitcode = 0;            
    int i;                       
    
    //mark if programm exits or not
    g_stopEvent = 0;             

     
    ngx_pid    = getpid();       
    ngx_parent = getppid();      
     
    g_argvneedmem = 0;
    for(i = 0; i < argc; i++)   
    {
        g_argvneedmem += strlen(argv[i]) + 1;  
    } 
     
    for(i = 0; environ[i]; i++) 
    {
        g_envneedmem += strlen(environ[i]) + 1;  
    }  

    g_os_argc = argc;            
    g_os_argv = (char **) argv;  

     
    ngx_log.fd = -1;                   
    ngx_process = NGX_PROCESS_MASTER;  
    ngx_reap = 0;                      
   
     
    //read out config file for use below 
    CConfig *p_config = CConfig::GetInstance();  
    if(p_config->Load("nginx.conf") == false)  
    {   
        ngx_log_init();   //log initialization  
        ngx_log_stderr(0,"config file[%s] load failure，exit!","nginx.conf");
         
        exitcode = 2;  
        goto lblexit;
    }
     
    CMemory::GetInstance();	
     
    CCRC32::GetInstance();
        
     //(4) Initialization
    ngx_log_init();         //initialize log     
        
     
    if(ngx_init_signals() != 0)  //initialize signal
    {
        exitcode = 1;
        goto lblexit;
    }        
    if(g_socket.Initialize() == false) //initialize socket
    {
        exitcode = 1;
        goto lblexit;
    }

     
    ngx_init_setproctitle();     

     
    //(6)create daemon process
    if(p_config->GetIntDefault("Daemon",0) == 1)  //read config file, and see if start as 'daemon process'
    {
        //if started as 'Daemon process', the daemon process will be the parent of the child worker processes 
        int cdaemonresult = ngx_daemon();
        if(cdaemonresult == -1)  //failed
        {
            exitcode = 1;     
            goto lblexit;
        }
        if(cdaemonresult == 1)
        {
             //origial master process
            freeresource();    
                               
            exitcode = 0;
            return exitcode;  //the whole process exists here 
        }
        //up to here, created successfully the dameon process which was fork()ed. This process is now master process 
        g_daemonized = 1;     
    }

    //(5) begin official main flow. The main flow is looping/runing in the following function
    //no matter master or child process, they always running in this function loop during normal operation
    ngx_master_process_cycle();  
        
        
lblexit:
     
    ngx_log_stderr(0,"program exit，bye!");
    freeresource();   
     
    return exitcode;
}

 
void freeresource()
{
    //free memory for setting up program name/title 
    if(gp_envmem)
    {
        delete []gp_envmem;
        gp_envmem = NULL;
    }

    //close log file 
    if(ngx_log.fd != STDERR_FILENO && ngx_log.fd != -1)  
    {        
        close(ngx_log.fd);  
        ngx_log.fd = -1;  
    }
}
