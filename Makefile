BUILD_DIR = ./build

.PHONY: all

all: $(BUILD_DIR)/server $(BUILD_DIR)/client

$(BUILD_DIR)/server: server.c $(BUILD_DIR)/libuipc.a
	cc -o $(BUILD_DIR)/server -Wall -Wextra -I./include/ server.c -L$(BUILD_DIR)/ -luipc

$(BUILD_DIR)/client: client.c $(BUILD_DIR)/libuipc.a
	cc -o $(BUILD_DIR)/client -Wall -Wextra -I./include/ client.c -L$(BUILD_DIR)/ -luipc

config:
	mkdir -p $(BUILD_DIR)/

clean:
	rm -rf $(BUILD_DIR)/
