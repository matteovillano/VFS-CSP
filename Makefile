CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
OBJ_DIR = obj

# Source files
SRCS_COMMON = $(wildcard src/common/*.c)
SRCS_SERVER = $(wildcard src/server/*.c)
SRCS_CLIENT = $(wildcard src/client/*.c)

# Object files
OBJS_COMMON = $(patsubst src/common/%.c, $(OBJ_DIR)/src/common/%.o, $(SRCS_COMMON))
OBJS_SERVER = $(patsubst src/server/%.c, $(OBJ_DIR)/src/server/%.o, $(SRCS_SERVER))
OBJS_CLIENT = $(patsubst src/client/%.c, $(OBJ_DIR)/src/client/%.o, $(SRCS_CLIENT))

# Targets
TARGET_SERVER = server
TARGET_CLIENT = client

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(OBJS_COMMON) $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET_CLIENT): $(OBJS_COMMON) $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^

# Pattern rule for object files
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJ_DIR) $(TARGET_SERVER) $(TARGET_CLIENT)

.PHONY: all clean
