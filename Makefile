# Makefile
#

CC = clang
CFLAGS = 
LFLAGS = 

TARGET = c4
SRC.c = c4.c
SRC.o = $(SRC.c:.c=.o)

BUILD_DIR = build
BIN_DIR = bin

vpath %.o $(BUILD_DIR)

PHONY += all
all: init $(TARGET)
	@echo "make all done"

PHONY += init
init:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

$(TARGET): $(SRC.o)
	@$(CC) -o $(BIN_DIR)/$@ $(SRC.o:%=$(BUILD_DIR)/%) $(LFLAGS)

%.o: %.c
	@$(CC) -c -o $(BUILD_DIR)/$@ $(CFLAGS) $<

PHONY += run
run: $(TARGET)
	@$(BIN_DIR)/$(TARGET) hello.c

clean:
	@$(RM) $(BUILD_DIR)/*

.PHONY : $(PHONY)
