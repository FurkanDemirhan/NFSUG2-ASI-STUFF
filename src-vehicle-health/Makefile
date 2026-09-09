CXX ?= i686-w64-mingw32-g++
CXXFLAGS ?= -m32 -shared -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas -Wno-deprecated-declarations -Wno-deprecated-copy -Wno-write-strings -Wno-implicit-fallthrough -static -static-libgcc -static-libstdc++ -I../includes -I.
LDFLAGS ?= -static -static-libgcc -static-libstdc++ -lkernel32 -luser32 -lgdi32 -lwinmm -ld3d9

TARGET_NAME = NFSU2VehicleHealth.asi
BUILD_DIR = ../build
OUTPUT_PATH = $(BUILD_DIR)/$(TARGET_NAME)
DEPLOY_DIR = ../GAME/PC/scripts
DEPLOY_PATH = $(DEPLOY_DIR)/$(TARGET_NAME)

SRCS = $(wildcard *.cpp)

all: $(OUTPUT_PATH)
	@if [ -d "$(DEPLOY_DIR)" ]; then \
		mkdir -p $(DEPLOY_DIR); \
		cp $(OUTPUT_PATH) $(DEPLOY_PATH); \
		if [ -f NFSU2VehicleHealth.ini ]; then cp NFSU2VehicleHealth.ini $(DEPLOY_DIR)/; fi; \
		echo "Deployed $(TARGET_NAME) to $(DEPLOY_DIR)/"; \
	fi

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OUTPUT_PATH): $(SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OUTPUT_PATH) $(DEPLOY_PATH)

.PHONY: all clean
