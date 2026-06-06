# --- Configuration ----------------------------------
BUILD   := build
SRC     := src
INCLUDE := src

# --- Flags for preprocessor, compiler, linker -------
CC       := gcc
CPPFLAGS := -I$(INCLUDE)
CFLAGS   := -std=gnu99
LDFLAGS  := -lrt -pthread -luuid

# Warnings
CFLAGS += -Wall -Wextra -pedantic
CFLAGS += -Wswitch-enum

# --- Input files ------------------------------------
SRCS := $(wildcard $(SRC)/*.c)
SHARED_SRCS := $(filter-out $(SRC)/client.c $(SRC)/server.c, $(SRCS))
OBJS := $(patsubst $(SRC)/%.c, $(BUILD)/%.o, $(SRCS))
SHARED_OBJS := $(patsubst $(SRC)/%.c, $(BUILD)/%.o, $(SHARED_SRCS))
DEPS := $(OBJS:.o=.d)

TARGETS := client server

.SECONDARY: $(OBJS)
.PHONY: all clean debug clean compdb

all: $(TARGETS)

%: $(BUILD)/%.o $(SHARED_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD)/client.o: $(SRC)/client.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/server.o: $(SRC)/server.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@


$(BUILD):
	mkdir -p $@

debug: CFLAGS+=-g -O0
debug: $(TARGETS)

# separate sanitized target to avoid conflicts
# with valgrind on debug target
# sanitized: LDFLAGS+=-fsanitize=thread
# sanitized: CFLAGS+=-fsanitize=thread
# sanitized: debug

clean:
	rm -rf client server $(BUILD)

# generate compile_commands.json
compdb:
	bear -- make

-include $(DEPS)
