
#.PHONY:all clean 

ifeq ($(DEBUG),true)

CC = g++ -std=c++11 -g 
VERSION = debug
else
CC = g++ -std=c++11
VERSION = release
endif

#CC = gcc


#SRCS = nginx.c ngx_conf.c
SRCS = $(wildcard *.cxx)

#OBJS = nginx.o ngx_conf.o  
OBJS = $(SRCS:.cxx=.o)

#DEPS = nginx.d ngx_conf.d
DEPS = $(SRCS:.cxx=.d)

#BIN = /mnt/hgfs/linux/nginx
BIN := $(addprefix $(BUILD_ROOT)/,$(BIN))


LINK_OBJ_DIR = $(BUILD_ROOT)/app/link_obj
DEP_DIR = $(BUILD_ROOT)/app/dep

$(shell mkdir -p $(LINK_OBJ_DIR))
$(shell mkdir -p $(DEP_DIR))


# /mnt/hgfs/linux/nginx/app/link_obj/nginx.o   /mnt/hgfs/linux/nginx/app/link_obj/ngx_conf.o 
OBJS := $(addprefix $(LINK_OBJ_DIR)/,$(OBJS))
DEPS := $(addprefix $(DEP_DIR)/,$(DEPS))


LINK_OBJ = $(wildcard $(LINK_OBJ_DIR)/*.o)
LINK_OBJ += $(OBJS)

#-------------------------------------------------------------------------------------------------------

all:$(DEPS) $(OBJS) $(BIN)


ifneq ("$(wildcard $(DEPS))","")   
include $(DEPS)  
endif

#----------------------------------------------------------------1begin------------------
#$(BIN):$(OBJS)
$(BIN):$(LINK_OBJ)
	@echo "------------------------build $(VERSION) mode--------------------------------!!!"


	$(CC) -o $@ $^ -lpthread

#----------------------------------------------------------------1end-------------------


#----------------------------------------------------------------2begin-----------------
#%.o:%.c
$(LINK_OBJ_DIR)/%.o:%.cxx

#$(CC) -o $@ -c $^
	$(CC) -I$(INCLUDE_PATH) -o $@ -c $(filter %.cxx,$^)
#----------------------------------------------------------------2end-------------------



#----------------------------------------------------------------3begin-----------------

$(DEP_DIR)/%.d:%.cxx
#gcc -MM $^ > $@

	echo -n $(LINK_OBJ_DIR)/ > $@
#	gcc -MM $^ | sed 's/^/$(LINK_OBJ_DIR)&/g' > $@

#	gcc -I$(INCLUDE_PATH) -MM $^ >> $@
	$(CC) -I$(INCLUDE_PATH) -MM $^ >> $@


#----------------------------------------------------------------4begin-----------------



#----------------------------------------------------------------nbegin-----------------
#clean:			
#	rm -f $(BIN) $(OBJS) $(DEPS) *.gch
#----------------------------------------------------------------nend------------------





