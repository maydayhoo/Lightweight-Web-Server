.PHONY: all clean

CC=g++
CCOPT=-MM

DIR_DEP=dep
DIR_SRC=src
DIR_OBJ=obj
SRCS=$(wildcard $(DIR_SRC)/*.cpp)
DEPS=$(patsubst $(DIR_SRC)/%.cpp,$(DIR_DEP)/%.dep,$(SRCS))

MKDIR=mkdir
MKDIROPT=-p

RM=rm
RMOPT=-rf

all: $(DIR_DEP) $(DEPS)

$(DIR_DEP):
	$(MKDIR) $(MKDIROPT) $@

$(DIR_DEP)/%.dep:$(DIR_SRC)/%.cpp
	$(CC) $(CCOPT) $^ | sed 's/^\([_[:alnum:]]\+\.o\)\(.*\)\(.cpp\)/$(DIR_OBJ)\/\1:/g' >> $@

clean:
	$(RM) $(RMOPT) $(DEPS)

