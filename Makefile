# --- Configuration ----------------------------------
BUILD_DIR   := build
SRC_DIR     := src
INCLUDE_DIR := src

# --- Flags for preprocessor, compiler, linker -------
CC       := gcc
CPPFLAGS := -I$(INCLUDE_DIR)
CFLAGS   := -std=gnu99
LDFLAGS  := -lrt -pthread -luuid -lm -lcrypto

# Warnings
CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -Wswitch-enum

# --- Input files ------------------------------------
SRCS := $(wildcard $(SRC_DIR)/*.c)
SHARED_SRCS := $(filter-out $(SRC_DIR)/client.c $(SRC_DIR)/server.c, $(SRCS))
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
SHARED_OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SHARED_SRCS))
DEPS := $(OBJS:.o=.d)

TARGETS := client server

.SECONDARY: $(OBJS)
.PHONY: all clean debug clean compdb

all: CFLAGS+=-DRELEASE
all: $(TARGETS)

%: $(BUILD_DIR)/%.o $(SHARED_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/client.o: $(SRC_DIR)/client.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/server.o: $(SRC_DIR)/server.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

debug: CFLAGS+=-g -O0 -fsanitize=undefined
debug: LDFLAGS+=-fsanitize=undefined
debug: $(TARGETS)

# separate sanitized target to avoid conflicts
# with valgrind on debug target
sanitized: LDFLAGS+=-fsanitize=address
sanitized: CFLAGS+=-fsanitize=address
sanitized: debug

clean:
	rm -rf client server $(BUILD_DIR)

# generate compile_commands.json
compdb:
	bear -- make

-include $(DEPS)

## Testing ##
.PHONY: test
test: test/main

.PHONY: test/main
test/main: CFLAGS+=-g
test/main: $(BUILD_DIR)/test.o $(SHARED_OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@
	./test/main
	rm ./test/main

.PHONY: $(BUILD_DIR)/test.o
$(BUILD_DIR)/test.o: test/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@
