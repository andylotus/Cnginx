//for child process related
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>    
#include <errno.h>     
#include <unistd.h>

#include "ngx_func.h"
#include "ngx_macro.h"
#include "ngx_c_conf.h"

//function declaration
static void ngx_start_worker_processes(int threadnums);
static int ngx_spawn_process(int threadnums,const char *pprocname);
static void ngx_worker_process_cycle(int inum,const char *pprocname);
static void ngx_worker_process_init(int inum);

 
static u_char  master_process[] = "master process";

//create worker child-process 
void ngx_master_process_cycle()
{    
    sigset_t set;         

    sigemptyset(&set);    
 
     //we don't want to receive the following signals by sigaddset(&set, SIGxx)
    sigaddset(&set, SIGCHLD);      
    sigaddset(&set, SIGALRM);      
    sigaddset(&set, SIGIO);        
    sigaddset(&set, SIGINT);       
    sigaddset(&set, SIGHUP);       
    sigaddset(&set, SIGUSR1);      
    sigaddset(&set, SIGUSR2);      
    sigaddset(&set, SIGWINCH);     
    sigaddset(&set, SIGTERM);      
    sigaddset(&set, SIGQUIT);      
     
    
    //block the above signals
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1)  
    {        
        ngx_log_error_core(NGX_LOG_ALERT,errno,"ngx_master_process_cycle()中sigprocmask()失败!");
    }
     
    //set up master process title/name
    size_t size;
    int    i;
    size = sizeof(master_process);   
    size += g_argvneedmem;           
    if(size < 1000)  
    {
        char title[1000] = {0};
        strcpy(title,(const char *)master_process);  
        strcat(title," ");   
        for (i = 0; i < g_os_argc; i++)          
        {
            strcat(title,g_os_argv[i]);
        } 
        ngx_setproctitle(title);  
        ngx_log_error_core(NGX_LOG_NOTICE,0,"%s %P 【master process】starts and begins to run......!",title,ngx_pid);  
    }    
     
        
    //get the number of worker processes from config file 
    // in my example, usually set it as 4 in the config file
    CConfig *p_config = CConfig::GetInstance();  
    int workprocess = p_config->GetIntDefault("WorkerProcesses",1); 
    //create worker child process 
    ngx_start_worker_processes(workprocess);   

     //after creating child processes, the master process will run to here, while child process cannot.
    sigemptyset(&set);  //clear set
    
    //master process is alwaying running here in this for-loop
    for ( ;; ) 
    {
        //master process is running totally depending on signals
        sigsuspend(&set);  

        sleep(1);  
    } 
    return;
}

 //run by master process
 //create child process with specified number (threadnums)
static void ngx_start_worker_processes(int threadnums)
{
    int i;
    for (i = 0; i < threadnums; i++)   //master process is executing this for-loop
    {
        ngx_spawn_process(i,"worker process");
    }  
    return;
}


 //create a child process
 //inum: the process number (start from 0)
 //pprocname: child process name/title
static int ngx_spawn_process(int inum,const char *pprocname)
{
    pid_t  pid;

    pid = fork();  
    switch (pid)   
    {  
    case -1:  
        ngx_log_error_core(NGX_LOG_ALERT,errno,"ngx_spawn_process()fork()creates child process num=%d,procname=\"%s\"failed!",inum,pprocname);
        return -1;

    case 0:   
        ngx_parent = ngx_pid;               
        ngx_pid = getpid();                 
        ngx_worker_process_cycle(inum,pprocname);  //all worker child processes will loop inside this function, not go to the control flow below   
        break;

    default:  
        break;
    } 

    //master process will come to here, while child process will not      
    return pid;
}
 
 
//only child process will execute this function
//every worker child process loops here to handle network event and timer event to provide web service 
//inum: the process number (start from 0)
 //pprocname: child process name/title
static void ngx_worker_process_cycle(int inum,const char *pprocname) 
{
     
    ngx_process = NGX_PROCESS_WORKER;   

     //rename child process to avoid the same name as master process
    ngx_worker_process_init(inum);
    ngx_setproctitle(pprocname);  
    ngx_log_error_core(NGX_LOG_NOTICE,0,"%s %P 【worker process】starts and begins to run......!",pprocname,ngx_pid);  
    
    for(;;)
    {
      

        ngx_process_events_and_timers();  


    }  

     
    g_threadpool.StopAll();       
    g_socket.Shutdown_subproc();  
    return;
}

 //child process initialization
static void ngx_worker_process_init(int inum)
{
    sigset_t  set;       

    sigemptyset(&set);   
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1)   
    {
        ngx_log_error_core(NGX_LOG_ALERT,errno,"ngx_worker_process_init(): sigprocmask() failed!");
    }

     
    CConfig *p_config = CConfig::GetInstance();
    int tmpthreadnums = p_config->GetIntDefault("ProcMsgRecvWorkThreadCount",5);  
    if(g_threadpool.Create(tmpthreadnums) == false)   
    {
         
        exit(-2);
    }
    sleep(1);  

    if(g_socket.Initialize_subproc() == false)  
    {
         
        exit(-2);
    }
    
    //epoll initialization 
    g_socket.ngx_epoll_init();            
  
    return;
}
