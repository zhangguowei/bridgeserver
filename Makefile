include ../pj/build.mak
include ../pj/version.mak
include $(PJDIR)/build/common.mak

export LIBDIR := ../pj/eice/lib
export BINDIR := ./bin

RULES_MAK := $(PJDIR)/build/rules.mak


PJLIB_LIB:=libpj-$(TARGET_NAME)$(LIBEXT)
PJLIB_UTIL_LIB:=libpjlib-util-$(TARGET_NAME)$(LIBEXT)
PJNATH_LIB:=libpjnath-$(TARGET_NAME)$(LIBEXT)

PJLIB_LIB_PATH:=$(PJDIR)/pjlib/lib/$(PJLIB_LIB)
PJLIB_UTIL_LIB_PATH:=$(PJDIR)/pjlib-util/lib/$(PJLIB_UTIL_LIB)
PJNATH_LIB_PATH:=$(PJDIR)/pjnath/lib/$(PJNATH_LIB)



MERG_LIBS:=$(PJLIB_LIB_PATH) $(PJLIB_UTIL_LIB_PATH) $(PJNATH_LIB_PATH) 

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


		   
SRC_DIRS = ../pj/eice/src/
HEAD_DIRS = $(SRC_DIRS)

INCLUDE_DIRS = $(addprefix -I,$(HEAD_DIRS)) 

###############################################################################
# Main entry
#
#
all:  $(TEST_BIN_PATH) $(CCS_BIN_PATH) $(BRIDGE_BIN_PATH)
.PHONY:all 



BRIDGESERVER_OBJS := $(wildcard ./bridgeserver/*.cpp)
$(BRIDGESERVER_OBJS):%.o:%.cpp
	$(CXX) -c -g -Ddebug $(_CXXFLAGS) $(APP_CFLAGS) $(INCLUDE_DIRS) $< -o $@
$(BRIDGE_BIN_PATH): $(BRIDGESERVER_OBJS) ./bridgeserver/main.cc
	mkdir -p $(BINDIR)
	$(APP_CXX) $^ -std=c++11 -L$(LIBDIR) -pthread  -lpthread -llog4cplus -levent -lrt -leice_full -luuid -o $@


TEST_OBJ := $(wildcard ./test/*.cpp)
$(TEST_OBJ):%.o:%.cpp
	$(CXX) -c -g -Ddebug $(_CXXFLAGS) $(APP_CFLAGS) $(INCLUDE_DIRS)  $< -o $@
$(TEST_BIN_PATH): $(TEST_OBJ) $(BRIDGESERVER_OBJS)
	mkdir -p $(BINDIR)
	$(APP_CXX)  $^ -g -Ddebug  -std=c++11 -L$(LIBDIR) -pthread  -lpthread -lcppunit -llog4cplus -levent -lrt -leice_full -luuid -o $@


$(CCS_BIN_PATH): $(CCS_OBJ) $(BRIDGESERVER_OBJS) ./test/mock_conference_control.cpp
	mkdir -p $(BINDIR)
	$(APP_CXX)  $^ -g -Ddebug -DMAIN  -std=c++11 -L$(LIBDIR) -pthread  -lpthread -lcppunit -llog4cplus -levent -lrt -leice_full -luuid -o $@


	
.PHONY: clean print_info
clean:	
	$(RM) $(TEST_OBJ)	
	$(RM) $(CCS_BIN_PATH)	
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
	





