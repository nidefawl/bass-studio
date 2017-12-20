.ONESHELL:
TARGET_EXEC ?= run/DAW.exe
COMPILER = clang

ifeq ($(COMPILER),clang)
CXX = clang++ -target x86_64-pc-windows-gnu
CC = clang -target x86_64-pc-windows-gnu
else
CXX = g++
CC = gcc
endif

AS = as
EDEV := E:/dev/libs
BUILD_DIR ?= build


SRC_DIR ?= src
SRC_TESTS_DIR ?= $(SRC_DIR)/test
EXCLUDE_PATHS := test res
#EXCLUDE_PATHS_ARG := $(EXCLUDE_PATHS:%=-and ! -path "$(SRC_DIR)/%/*")
EXCLUDE_PATHS_ARG := -and ! -path "$(SRC_DIR)/test/test.cpp"
#$(info EXCLUDE_PATHS_ARG="$(EXCLUDE_PATHS_ARG)")

SRCS := $(shell find $(SRC_DIR) -type f \( -name *.cpp -or -name *.c -or -name *.s \) $(EXCLUDE_PATHS_ARG))
SRCS_TEST := $(shell find $(SRC_TESTS_DIR) -name *.cpp -or -name *.c -or -name *.s)
#$(info SRCS="$(SRCS)")
RELPATHS := $(SRCS:$(SRC_DIR)/%=%)
RELPATHS_TESTS := $(SRCS_TEST:$(SRC_TESTS_DIR)/%=test/%)

#$(info SRCS_TEST="$(SRCS_TEST)")

OBJS := $(RELPATHS:%=$(BUILD_DIR)/%.o)
OBJS_SCANNER := $(filter-out build/host/main.cpp.o,$(OBJS))
OBJS_MAIN := $(filter-out build/host/scanner.cpp.o,$(OBJS))
OBJS_TEST := $(filter-out build/host/main.cpp.o build/host/scanner.cpp.o,$(OBJS))
OBJS_TEST += $(RELPATHS_TESTS:%=$(BUILD_DIR)/%.o)
OBJS += $(RELPATHS_TESTS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)


INC_DIRS += $(EDEV)/cereal \
$(EDEV)/glad/include \
$(EDEV)/glad/src \
$(EDEV)/glm \
$(EDEV)/nanovg \
$(EDEV)/glfw_mingw64/include \
$(EDEV)/PortAudio_190600/include \
$(EDEV)/PortAudio_190600/src/common \
$(EDEV)/PortAudio_190600/src/os/win \
$(EDEV)/PortAudio_190600/src/hostapi/asio \
$(EDEV)/PortAudio_190600/src/hostapi/dsound \
$(EDEV)/ASIOSDK2.3/common \
$(EDEV)/ASIOSDK2.3/host \
$(EDEV)/ASIOSDK2.3/host/pc \
$(EDEV)/SQLiteCpp/include

#$(info OBJS_MAIN="$(OBJS_MAIN)")
#$(info OBJS_SCANNER="$(OBJS_SCANNER)")
INC_FLAGS := $(addprefix -isystem ,$(INC_DIRS)) -iquote $(SRC_DIR)/include

ifeq ($(COMPILER),clang)
LIB_DIRS += $(EDEV)/glfw_mingw64/lib \
$(EDEV)/SQLiteCpp_clang/lib
else
LIB_DIRS += $(EDEV)/glfw_mingw64/lib \
$(EDEV)/SQLiteCpp/lib
endif
#-Ofast
OPTIMIZATION_LVL ?= -O0
DEBUG_FLAGS ?= -g3
LD_FLAGS := $(addprefix -L,$(LIB_DIRS)) -lglfw3 -lwinmm -lkernel32 -lgdi32 -lole32 -luuid -lcomdlg32 -lSQLiteCpp -lsqlite3
LD_FLAGS += $(OPTIMIZATION_LVL) -Wall $(DEBUG_FLAGS)
#LD_FLAGS := libs.o -lole32
#-DTEST_PROJECT=1 
NO_WARNINGS := -Wno-inconsistent-missing-override
COMPILE_SYMBOLS ?= -DPA_USE_ASIO=1 -DPA_USE_DS=1 -DVST_FORCE_DEPRECATED=0
CFLAGS ?= $(INC_FLAGS) -fmessage-length=0 -Wall $(NO_WARNINGS) $(DEBUG_FLAGS) -MMD -MP $(OPTIMIZATION_LVL) $(COMPILE_SYMBOLS) 
CPPFLAGS ?= $(CFLAGS) -std=c++14

all: $(TARGET_EXEC)
scanner: Host_vstscanner.exe

tests.exe: $(OBJS_TEST)
	$(CXX) $(OBJS_TEST) $(LD_FLAGS) -o $@ 

$(TARGET_EXEC): $(OBJS_MAIN)
	$(CXX) $(OBJS_MAIN) $(LD_FLAGS) -o $@ 

Host_vstscanner.exe: $(OBJS_SCANNER)
	$(CXX) $(OBJS_SCANNER) $(LD_FLAGS) -o $@ 
# assembly
$(BUILD_DIR)/%.s.o: %.s
	@echo "${AS} $@"
	@$(MKDIR_P) $(dir $@)
	@$(AS) $(ASFLAGS) -c $< -o $@

# c source
$(BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	@echo "${CC} $@"
	@$(MKDIR_P) $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: $(SRC_DIR)/%.cpp
	@echo "Compile: ${COMPILER}++ $@"
	@$(MKDIR_P) $(dir $@)
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


.PHONY: all clean link scanner

link:
	rm -f $(BUILD_DIR)/$(TARGET_EXEC)
	make $(BUILD_DIR)/$(TARGET_EXEC) 

clean:
	rm -Rf $(BUILD_DIR)
test: tests.exe
	tests.exe

-include $(DEPS)

MKDIR_P ?= mkdir.exe -p
