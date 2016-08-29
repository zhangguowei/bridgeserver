CC=g++ -std=c++0x -g -DDEBUG -O0 -fprofile-arcs -ftest-coverage -fsanitize=address -fno-omit-frame-pointer
COMMON_INC = ../../../common/include/
LIB_DIR = ../../../common/lib ../../lib
INCLUDES=-I. -I../../../common/include -I../ -I../../include/
LIBS = -fsanitize=address -fno-omit-frame-pointer  -llog4cplus -lcppunit -levent 
DEPS = $(COMMON_INC)*.h ../include/*.h ../*.h
CPP_OBJ = $(foreach cpp, ./, $(addsuffix .o, $(basename $(cpp))))		

testbridge : $(CPP_OBJ)
	$(CC) -o $@ $^ $(addprefix -L,$(LIB_DIR)) $(LIBS)
$(CPP_OBJ): %.o : %.cpp
	$(CC) -c $(INCLUDES) $< -o $@
	
clean:
	rm -f testbridge $(CPP_OBJ)
gcovclean:
	rm -f *.gcno *.gcda *.gcov
