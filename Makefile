
PJ_DIR_ := ..
ifdef PJ_DIR	
	PJ_DIR_ := $(PJ_DIR)
endif

include $(PJ_DIR_)/build.mak
include $(PJ_DIR_)/version.mak
include $(PJDIR)/build/common.mak

export LIBDIR := ./lib
export BINDIR := ./bin

RULES_MAK := $(PJDIR)/build/rules.mak

export TARGET_LIB := libeice-$(TARGET_NAME)$(LIBEXT)

PJLIB_LIB:=libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=libpjlib-util-$(TARGET_NAME)$(LIBEXT)
PJNATH_LIB:=libpjnath-$(TARGET_NAME)$(LIBEXT)

PJLIB_LIB_PATH:=$(PJDIR)/pjlib/lib/$(PJLIB_LIB)
PJLIB_UTIL_LIB_PATH:=$(PJDIR)/pjlib-util/lib/$(PJLIB_UTIL_LIB)
PJNATH_LIB_PATH:=$(PJDIR)/pjnath/lib/$(PJNATH_LIB)



ifeq ($(EICE_SHARED_LIBRARIES),)
else
TARGET_SONAME = libeice.$(SHLIB_SUFFIX)
export EICE_SONAME := libeice.$(SHLIB_SUFFIX)
export EICE_SHLIB := $(TARGET_SONAME).$(PJ_VERSION_MAJOR)
endif

MERG_LIBS:=$(PJLIB_LIB_PATH) $(PJLIB_UTIL_LIB_PATH) $(PJNATH_LIB_PATH) $(LIBDIR)/$(TARGET_LIB)
ARSCRIPT:=_tmp.ar
ARSCRIPT_PATH:=$(LIBDIR)/$(ARSCRIPT)
FULL_LIB_PATH:=$(LIBDIR)/libeice_.a

TEST_BIN_PATH:=$(BINDIR)/test.bin
CCS_BIN_PATH:=$(BINDIR)/ccs
BRIDGE_BIN_PATH:=$(BINDIR)/bridge

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC)../include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(APP_LDFLAGS) $(LDFLAGS) 


###############################################################################
# Defines for building library
#
		   
SRC_DIRS = third/eice/src/
HEAD_DIRS = $(SRC_DIRS)

#functions
get_files =$(foreach dir,$(subst $(DIR_DIVIDER),$(MK_DIR_DIVIDER),$(1)),$(wildcard $(dir)$(MK_DIR_DIVIDER)$(2)))

C_SRC = $(call get_files,$(SRC_DIRS),*.c)
CPP_SRC = $(call get_files,$(SRC_DIRS),*.cpp)
INCLUDE_DIRS = $(addprefix -I,$(HEAD_DIRS)) 


C_OBJ = $(foreach src, $(C_SRC), $(addsuffix .o, $(basename $(src))))
CPP_OBJ = $(foreach cpp, $(CPP_SRC), $(addsuffix .o, $(basename $(cpp))))

###############################################################################
# Main entry
#
#
all: $(LIBDIR)/$(TARGET_LIB) $(FULL_LIB_PATH) $(TEST_BIN_PATH) $(CCS_BIN_PATH) $(BRIDGE_BIN_PATH)
.PHONY:all 

$(C_OBJ):%.o:%.c
	$(CC) -c -Ddebug $(_CFLAGS) $(APP_CFLAGS) $(INCLUDE_DIRS)  $< -o $@

$(CPP_OBJ):%.o:%.cpp
	$(CXX) -c $(_CXXFLAGS) $(APP_CFLAGS) $(INCLUDE_DIRS) $< -o $@
$(LIBDIR)/$(TARGET_LIB): $(C_OBJ) $(CPP_OBJ)
	mkdir -p $(LIBDIR)
#	$(CC) $(C_OBJ) $(CPP_OBJ) $(_LDFLAGS) -o $@
	$(AR) rv $@ $(C_OBJ) $(CPP_OBJ) 
	@echo make success

$(FULL_LIB_PATH): $(MERG_LIBS)
	$(SILENT)echo "CREATE $@" > $(ARSCRIPT_PATH)
	$(SILENT)for a in $(MERG_LIBS); do (echo "ADDLIB $$a" >> $(ARSCRIPT_PATH)); done
#	$(SILENT)echo "ADDMOD $(OBJECTS)" >> $(ARSCRIPT_PATH)
	$(SILENT)echo "SAVE" >> $(ARSCRIPT_PATH)
	$(SILENT)echo "END" >> $(ARSCRIPT_PATH)
	$(SILENT)$(AR) -M < $(ARSCRIPT_PATH)
	$(RM) $(ARSCRIPT_PATH)
	

BRIDGESERVER_SRCS := $(wildcard ./bridgeserver/*.cpp)
BRIDGESERVER_OBJS = $(foreach cpp, $(BRIDGESERVER_SRCS), $(addsuffix .o, $(basename $(cpp))))
$(BRIDGESERVER_OBJS):%.o:%.cpp
	$(CXX) -c -g -Ddebug -std=c++11 $(_CXXFLAGS) $(APP_CFLAGS) $(INCLUDE_DIRS) $< -o $@
$(BRIDGE_BIN_PATH): $(BRIDGESERVER_OBJS) ./bridgeserver/main.cc
	mkdir -p $(BINDIR)
	$(APP_CXX) $^ -std=c++11  -L$(LIBDIR) -pthread  -lpthread -llog4cplus -levent -lrt -leice_ -luuid -o $@


TEST_SRCS := $(wildcard ./test/*.cpp) 
TEST_OBJS = $(foreach cpp, $(TEST_SRCS), $(addsuffix .o, $(basename $(cpp))))
$(TEST_OBJS):%.o:%.cpp	
	$(CXX) -c -g -Ddebug -std=c++11  $(_CXXFLAGS) $(APP_CFLAGS) $(INCLUDE_DIRS)  $< -o $@
$(TEST_BIN_PATH): $(TEST_OBJS) $(BRIDGESERVER_OBJS)
	mkdir -p $(BINDIR)
	$(APP_CXX)  $^ -g -Ddebug  -std=c++11  -L$(LIBDIR) -pthread  -lpthread -lcppunit -llog4cplus -levent -lrt -leice_  -luuid -o $@



CCS_OBJ = ./test/mock_conference_control_main.o
$(CCS_OBJ):./test/mock_conference_control.cpp
	$(CXX) -c -g -Ddebug -DMAIN -std=c++11  $(_CXXFLAGS) $(APP_CFLAGS) $(INCLUDE_DIRS)  $< -o $@
$(CCS_BIN_PATH): $(CCS_OBJ) $(BRIDGESERVER_OBJS) 
	mkdir -p $(BINDIR)
	$(APP_CXX)  $^ -g -Ddebug -std=c++11  $(INCLUDE_DIRS) -L$(LIBDIR) -pthread  -lpthread -lcppunit -llog4cplus -levent -lrt -leice_  -luuid -o $@


	
.PHONY: clean print_info
clean:		
	$(RM) $(C_OBJ)
	$(RM) $(CPP_OBJ)
	$(RM) $(TEST_OBJS)
	$(RM) $(CCS_OBJS)
	$(RM) $(BRIDGESERVER_OBJS)
	$(RM) $(BRIDGE_BIN_PATH)	
	$(RM) $(TEST_BIN_PATH)

print:
	@echo TARGET_PLATFORM=$(TARGET_PLATFORM)
	@echo ----------------------
	@echo CC=$(CC)
	@echo ----------------------
	@echo CFLAGS=$(_CFLAGS)
	@echo ----------------------
	@echo CPPFLAGS=$(_CXXFLAGS)
	@echo ----------------------
	@echo C_SRC=$(C_SRC)
	@echo ----------------------
	@echo CPP_SRC=$(CPP_SRC)
	@echo ----------------------
	@echo C_OBJ=$(C_OBJ)
	@echo ----------------------
	@echo CPP_OBJ=$(CPP_OBJ)
	





