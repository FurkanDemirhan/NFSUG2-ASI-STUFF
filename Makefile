CXX ?= i686-w64-mingw32-g++
CXXFLAGS ?= -m32 -shared -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas -Wno-deprecated-declarations -Wno-deprecated-copy -Wno-write-strings -Wno-implicit-fallthrough -static -static-libgcc -static-libstdc++ -I./src/includes -I./src
LDFLAGS ?= -static -static-libgcc -static-libstdc++ -lkernel32 -luser32 -lgdi32 -lwinmm -ld3d9

TARGET_NAME = NFSU2CodeRestoration.asi
HEALTH_TARGET_NAME = NFSU2VehicleHealth.asi

BUILD_DIR = build
OUTPUT_PATH = $(BUILD_DIR)/$(TARGET_NAME)
DEPLOY_PATH = GAME/PC/scripts/$(TARGET_NAME)

HEALTH_OUTPUT_PATH = $(BUILD_DIR)/$(HEALTH_TARGET_NAME)
HEALTH_DEPLOY_PATH = GAME/PC/scripts/$(HEALTH_TARGET_NAME)

SRCS = $(wildcard src/*.cpp) $(wildcard src/restorations/*.cpp)
HEALTH_SRCS = $(wildcard src-vehicle-health/*.cpp)

all: $(DEPLOY_PATH) $(HEALTH_DEPLOY_PATH)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OUTPUT_PATH): $(SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(DEPLOY_PATH): $(OUTPUT_PATH)
	mkdir -p GAME/PC/scripts
	cp $(OUTPUT_PATH) $(DEPLOY_PATH)
	@echo "Deployed $(TARGET_NAME) to GAME/PC/scripts/"

$(HEALTH_OUTPUT_PATH): $(HEALTH_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I./src-vehicle-health -o $@ $^ $(LDFLAGS)

$(HEALTH_DEPLOY_PATH): $(HEALTH_OUTPUT_PATH)
	mkdir -p GAME/PC/scripts
	cp $(HEALTH_OUTPUT_PATH) $(HEALTH_DEPLOY_PATH)
	@echo "Deployed $(HEALTH_TARGET_NAME) to GAME/PC/scripts/"

clean:
	rm -rf $(BUILD_DIR) $(DEPLOY_PATH) $(HEALTH_DEPLOY_PATH)

.PHONY: all clean

