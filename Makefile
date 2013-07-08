BUILD_DIR := ./build
OBJ_DIR := $(BUILD_DIR)/obj

MAIN_OBJECTS = $(addprefix $(OBJ_DIR)/, ftp_main.o)

ALL_OBJECTS = $(MAIN_OBJECTS)


INC_DIRS = ./src

LIBS := pthread
CC_FLAGS := -W -Wall -Wextra -pedantic -fstrict-aliasing -std=c99 -O0 -ggdb


default: ftpserver

ftpserver: $(OBJ_DIR) $(BUILD_DIR)/ftpserver


$(BUILD_DIR)/ftpserver: $(MAIN_OBJECTS)
	$(CC) $(CC_FLAGS) -o $@ $^ $(addprefix -L, $(ORA_LIB_DIRS)) $(addprefix -l, $(LIBS))

# pull in dependency info for existing .o files
-include $(ALL_OBJECTS:.o=.d)

$(OBJ_DIR)/%.o: src/%.c
	$(CC) $(CC_FLAGS) $(addprefix -I, $(INC_DIRS)) -c -o $@ $<
	-@$(CC) $(addprefix -I, $(INC_DIRS)) -MT '$@' -MM $< > $(@:.o=.d)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

.PHONY: clean
clean:
	-rm -f $(ALL_OBJECTS) $(ALL_OBJECTS:.o=.d)
	-rm -f $(BUILD_DIR)/ftpserver
