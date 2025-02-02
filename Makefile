SRC=src
INC=include
LIB=lib
OBJ=obj
BIN=bin

LIBS=$(patsubst $(LIB)/lib%.a, -l%, $(wildcard $(LIB)/*.a))
OBJS=$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(wildcard $(SRC)/*.c)) 

CC=gcc
CFLAGS=-I$(INC) -Wall -O3
LDFLAGS=-L$(LIB) $(LIBS)

.PHONY: all clean install

all: $(BIN)/sentinel

$(BIN)/sentinel: $(OBJS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $? $(LDFLAGS)

$(OBJ)/main.o: $(SRC)/main.c | $(OBJ)
	$(CC) $(CFLAGS) -c -o $@ $< 

$(OBJ)/utils.o: $(SRC)/utils.c | $(OBJ)
	$(CC) $(CFLAGS) -c -o $@ $< 
	
$(OBJ)/cache.o: $(SRC)/cache.c | $(OBJ)
	$(CC) $(CFLAGS) -c -o $@ $< 

$(OBJ):
	mkdir -p $@

$(BIN):
	mkdir -p $@

clean:
	rm -rf $(OBJ)
	rm -rf $(BIN)
