//to start child/worker processes 
 
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

 
void ngx_process_events_and_timers()
{
    g_socket.ngx_epoll_process_events(-1);  

     
}

