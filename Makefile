CXX ?= i686-w64-mingw32-g++
BASE_CXXFLAGS = -m32 -shared -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas -Wno-deprecated-declarations -Wno-deprecated-copy -Wno-write-strings -Wno-implicit-fallthrough -static -static-libgcc -static-libstdc++
LDFLAGS ?= -static -static-libgcc -static-libstdc++ -lkernel32 -luser32 -lgdi32 -lwinmm -ld3d9

RESTORE_CXXFLAGS = $(BASE_CXXFLAGS) -I./includes -I./src-CodeRestoreTest/includes -I./src-CodeRestoreTest
HEALTH_CXXFLAGS = $(BASE_CXXFLAGS) -I./includes -I./src-vehicle-health

RESTORE_TARGET = NFSU2CodeRestoration.asi
HEALTH_TARGET = NFSU2VehicleHealth.asi

BUILD_DIR = build
RESTORE_OUTPUT = $(BUILD_DIR)/$(RESTORE_TARGET)
HEALTH_OUTPUT = $(BUILD_DIR)/$(HEALTH_TARGET)

SCRIPTS_DIR = GAME/PC/scripts
RESTORE_DEPLOY = $(SCRIPTS_DIR)/$(RESTORE_TARGET)
HEALTH_DEPLOY = $(SCRIPTS_DIR)/$(HEALTH_TARGET)

RESTORE_SRCS = $(wildcard src-CodeRestoreTest/*.cpp) $(wildcard src-CodeRestoreTest/restorations/*.cpp)
HEALTH_SRCS = $(wildcard src-vehicle-health/*.cpp)

all: code-restore vehicle-health

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# --- Code Restoration (Experimental) ---
code-restore: $(RESTORE_DEPLOY)

restore: code-restore

$(RESTORE_OUTPUT): $(RESTORE_SRCS) | $(BUILD_DIR)
	$(CXX) $(RESTORE_CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(RESTORE_DEPLOY): $(RESTORE_OUTPUT)
	@mkdir -p $(SCRIPTS_DIR)
	cp $(RESTORE_OUTPUT) $(RESTORE_DEPLOY)
	@if [ -f src-CodeRestoreTest/NFSU2CodeRestoration.ini ]; then cp src-CodeRestoreTest/NFSU2CodeRestoration.ini $(SCRIPTS_DIR)/; fi
	@echo "Deployed $(RESTORE_TARGET) to $(SCRIPTS_DIR)/"

# --- Vehicle Health System ---
vehicle-health: $(HEALTH_DEPLOY)

health: vehicle-health

$(HEALTH_OUTPUT): $(HEALTH_SRCS) | $(BUILD_DIR)
	$(CXX) $(HEALTH_CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(HEALTH_DEPLOY): $(HEALTH_OUTPUT)
	@mkdir -p $(SCRIPTS_DIR)
	cp $(HEALTH_OUTPUT) $(HEALTH_DEPLOY)
	@if [ -f src-vehicle-health/NFSU2VehicleHealth.ini ]; then cp src-vehicle-health/NFSU2VehicleHealth.ini $(SCRIPTS_DIR)/; fi
	@echo "Deployed $(HEALTH_TARGET) to $(SCRIPTS_DIR)/"

clean:
	rm -rf $(BUILD_DIR) $(RESTORE_DEPLOY) $(HEALTH_DEPLOY)

.PHONY: all clean code-restore restore vehicle-health health
