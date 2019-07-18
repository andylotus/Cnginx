
#ifndef __NGX_COMM_H__
#define __NGX_COMM_H__

//maximum length of each pack = header + body 
#define _PKG_MAX_LENGTH     30000   

 //status of receiving pack
#define _PKG_HD_INIT         0   //initial state, begin to receive pack
#define _PKG_HD_RECVING      1   //during the receiving of header
#define _PKG_BD_INIT         2   //just completed receiving header, begin to receive body
#define _PKG_BD_RECVING      3   //during receiving body
 
//buffer to receive pack header. size >sizeof(COMM_PKG_HEADER)
#define _DATA_BUFSIZE_       20   
                                   

//define the way of alignment in the struct for the pack header
#pragma pack (1)  

typedef struct _COMM_PKG_HEADER
{
	unsigned short pkgLen;    //2 byts, total length of pack (header + body) 
	                             
	unsigned short msgCode;   //2 bytes, mesg type 
	int            crc32;     //4 bytes, crc32 check 
}COMM_PKG_HEADER,*LPCOMM_PKG_HEADER;

#pragma pack()  


#endif
