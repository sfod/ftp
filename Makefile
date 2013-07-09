BUILD_DIR := ./build
OBJ_DIR := $(BUILD_DIR)/obj

SERVER_OBJECTS = $(addprefix $(OBJ_DIR)/, ftp_server.o)
UTIL_OBJECTS = $(addprefix $(OBJ_DIR)/, ftp_proto.o)
TEST_OBJECTS = $(addprefix $(OBJ_DIR)/, test_ftp_proto.o)

ALL_OBJECTS = $(SERVER_OBJECTS) $(UTIL_OBJECTS) $(TEST_OBJECTS)


INC_DIRS = ./src

LIBS := pthread
CC_FLAGS := -W -Wall -Wextra -pedantic -fstrict-aliasing -std=c99 -O0 -ggdb


default: ftpserver

ftpserver: $(OBJ_DIR) $(BUILD_DIR)/ftpserver
test_ftp_proto: $(OBJ_DIR) $(BUILD_DIR)/test_ftp_proto


$(BUILD_DIR)/ftpserver: $(SERVER_OBJECTS) $(UTIL_OBJECTS)
	$(CC) $(CC_FLAGS) -o $@ $^ $(addprefix -l, $(LIBS))

$(BUILD_DIR)/test_ftp_proto: $(TEST_OBJECTS) $(UTIL_OBJECTS)
	$(CC) $(CC_FLAGS) -o $@ $^ $(addprefix -l, $(LIBS))

# pull in dependency info for existing .o files
-include $(ALL_OBJECTS:.o=.d)

$(OBJ_DIR)/%.o: src/%.c
	$(CC) $(CC_FLAGS) $(addprefix -I, $(INC_DIRS)) -c -o $@ $<
	-@$(CC) $(addprefix -I, $(INC_DIRS)) -MT '$@' -MM $< > $(@:.o=.d)

$(OBJ_DIR)/%.o: t/%.c
	$(CC) $(CC_FLAGS) $(addprefix -I, $(INC_DIRS)) -c -o $@ $<
	-@$(CC) $(addprefix -I, $(INC_DIRS)) -MT '$@' -MM $< > $(@:.o=.d)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

.PHONY: clean
clean:
	-rm -f $(ALL_OBJECTS) $(ALL_OBJECTS:.o=.d)
	-rm -f $(BUILD_DIR)/ftpserver
	-rm -f $(BUILD_DIR)/test_ftp_proto
