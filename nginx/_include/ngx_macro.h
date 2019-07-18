
#ifndef __NGX_MACRO_H__
#define __NGX_MACRO_H__

 //#define macros 
#define NGX_MAX_ERROR_STR   2048    //max array size for error

//similar to memcpy
#define ngx_cpymem(dst, src, n)   (((u_char *) memcpy(dst, src, n)) + (n))   
#define ngx_min(val1, val2)  ((val1 > val2) ? (val2) : (val1))               

 
#define NGX_MAX_UINT32_VALUE   (uint32_t) 0xffffffff               
#define NGX_INT64_LEN          (sizeof("-9223372036854775808") - 1)     

//log related
#define NGX_LOG_STDERR            0     
#define NGX_LOG_EMERG             1     
#define NGX_LOG_ALERT             2     
#define NGX_LOG_CRIT              3     
#define NGX_LOG_ERR               4     
#define NGX_LOG_WARN              5     
#define NGX_LOG_NOTICE            6     
#define NGX_LOG_INFO              7     
#define NGX_LOG_DEBUG             8     

//default path for error log file
#define NGX_ERROR_LOG_PATH       "error.log"    
 
 
#define NGX_PROCESS_MASTER     0   //master process
#define NGX_PROCESS_WORKER     1   //worker process
 

#endif
