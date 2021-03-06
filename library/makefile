# UNIVERSAL MAKEFILE FOR STATIC LIBRARIES
# Credits: Massimo "Keebus" Tristano <massimo.tristano@gmail.com>
# Licensed under GNU GPL license.

TARGET_NAME := IonScript

SOURCE_DIR := source
BIN_DIR := bin
BUILD_DIR := build
INCLUDE_DIR := include

#Release Configuration. Simply type "make" to compile with this configuration.
CFLAGS := -O3 -Wall

#Debug Configuration. Type "make debug" to compile with this configuration.
CFLAGS_D := -g -O0 -Wall

#######DONT EDIT THIS PART IF YOU DONT KNOW EXACTLY WHAT YOU'RE DOING###########
CC = g++

INCLUDES := -I$(SOURCE_DIR)

TARGET:= $(BIN_DIR)/lib$(TARGET_NAME).a
TARGET_D := $(BIN_DIR)/lib$(TARGET_NAME)_d.a

SRC := $(shell find ./$(SOURCE_DIR)/ -name "*.cpp" -print)
OBJS := $(SRC:./$(SOURCE_DIR)/%.cpp=./$(BUILD_DIR)/release/%.o)
OBJS_D := $(SRC:./$(SOURCE_DIR)/%.cpp=./$(BUILD_DIR)/debug/%.o)

SRCDIR := $(shell find ./$(SOURCE_DIR) -type d -exec ls -d {} \;)
DIR :=$(SRCDIR:./$(SOURCE_DIR)%=./$(BUILD_DIR)/release%)
DIR_D := $(SRCDIR:./$(SOURCE_DIR)%=./$(BUILD_DIR)/debug%)

SRCHEADERS := $(shell find ./$(SOURCE_DIR)/ -name "*.h" -print)
HEADERS := $(SRCHEADERS:./$(SOURCE_DIR)/%=./$(INCLUDE_DIR)/%)
HEADERS_DIR := $(SRCDIR:./$(SOURCE_DIR)%=./$(INCLUDE_DIR)%)

CFLAGS +=
CFLAGS_D += -DDEBUG

###########ALL##############
all: release debug headers
	@echo ">> Done :)"

##########RELEASE############
release: release-info directories $(TARGET)

release-info:
	@echo ">> Building $(TARGET) with the release configuration..."

directories:
	@mkdir -p $(DIR) $(BIN_DIR)/$(PLATFORM) $(BUILD_DIR)

$(TARGET) : $(OBJS)
	@echo ">> Making release static library..."
	@ar rcs $(TARGET) $(OBJS)

$(OBJS): ./$(BUILD_DIR)/release/%.o : ./$(SOURCE_DIR)/%.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

##########DEBUG############
debug: debug-info directories_d $(TARGET_D)
	@echo ">> Done :)"

debug-info:
	@echo ">> Building $(TARGET_NAME) with the debug configuration..."

directories_d:
	@mkdir -p $(DIR_D) $(BIN_DIR) $(BUILD_DIR)

$(TARGET_D) : $(OBJS_D)
	@echo ">> Making static static library..."
	@ar rcs $(TARGET_D) $(OBJS_D)

$(OBJS_D): ./$(BUILD_DIR)/debug/%.o : ./$(SOURCE_DIR)/%.cpp
	$(CC) $(CFLAGS_D) $(INCLUDES) -c $< -o $@

##########HEADERS##########
headers: headers-info headers-directory $(HEADERS)
	@echo ">> Headers copied."

headers-info:
	@echo ">> Making $(INCLUDE_DIR) directory and copying headers..."

headers-directory:
	@rm -rf $(INCLUDE_DIR)/script
	-mkdir -p $(HEADERS_DIR)

$(HEADERS): ./$(INCLUDE_DIR)/% : ./$(SOURCE_DIR)/%
	@cp $< $@

##########EXTRA###########
clean:
	@echo ">> Cleaning..."
	-rm -rf $(BIN_DIR) $(BUILD_DIR) $(INCLUDE_DIR)
	-rmdir $(BIN_DIR) $(BUILD_DIR)
	@echo ">> Done :)"

polish:
	@echo ">> Polishing..."
	-rm -rf $(BUILD_DIR)
	-rmdir $(BUILD_DIR)
	@echo ">> Done :)"

package: all
	@echo ">> Making package..."
	-tar czf lib$(TARGET_NAME)-$(VERSION).tar.gz $(BIN_DIR) $(INCLUDE_DIR) README

install: all
	@echo ">> Installing..."
	cp ./$(INCLUDE_DIR)/* /usr/include/ -r
	cp $(TARGET) $(TARGET_D) /usr/lib
