.PHONY: all clean

cc=g++
COMPILE_OPT=-g

DIR_SRC=src
DIR_HEADER=include
DIR_OBJ=obj
DIR_DEP=dep

SRCS=$(wildcard $(DIR_SRC)/*.cpp)
OBJS=$(patsubst $(DIR_SRC)/%.cpp,$(DIR_OBJ)/%.o, $(SRCS))
DEPS=$(patsubst $(DIR_SRC)/%.cpp, $(addprefix $(DIR_DEP)/,%.dep), $(SRCS))

RM=rm
RMOPTS=-rf
MKDIR=mkdir
MKDIROPT=-p

EXE=main.exe
DIR_EXE=exe
EXE:=$(addprefix $(DIR_EXE)/,$(EXE))

DIRS=$(DIR_OBJ) $(DIR_EXE)

all: $(DIRS) $(EXE)
-include deps.mk
-include $(DEPS)

$(DIRS):
	$(MKDIR) $(MKDIROPT) $@

$(EXE): $(OBJS)
	$(CC) -o $@ $^ -pthread

$(DIR_OBJS)/%.o: $(DIR_SRC)/%.cpp
	$(CC) -c $(filter %.cpp, %^) -o $@

clean:
	$(RM) $(RMOPTS) $(DIR_EXE) $(DIR_OBJ) $(DIR_DEP)

