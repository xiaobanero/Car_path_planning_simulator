# g++ 直接编译（不依赖 CMake）
# UCRT64（推荐）：在「MSYS2 UCRT64」终端安装
#   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf mingw-w64-ucrt-x86_64-pkgconf
# MinGW64：用 mingw-w64-x86_64-* 包，并在 MinGW64 终端执行 make
#
# 未装 pkg-config 时默认前缀为 UCRT64；若你用 mingw64，请：
#   mingw32-make MINGW_PREFIX=D:/msys2/mingw64

CXX := g++
SRC := src/main.cpp
CXXFLAGS := -std=c++17 -Wall -O2

ifeq ($(OS),Windows_NT)
  TARGET := path_planning_sim.exe
else
  TARGET := path_planning_sim
endif

PC_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf 2>/dev/null)
PC_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf 2>/dev/null)

ifneq ($(strip $(PC_LIBS)),)
  EXTRA_CXXFLAGS := $(PC_CFLAGS)
  LDLIBS := $(PC_LIBS)
else
  ifeq ($(OS),Windows_NT)
    MINGW_PREFIX ?= D:/msys2/ucrt64
    EXTRA_CXXFLAGS := -I$(MINGW_PREFIX)/include/SDL2
    LDLIBS := -L$(MINGW_PREFIX)/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf
  else
    $(error 未检测到 pkg-config 的 sdl2/SDL2_ttf。请安装 SDL2/SDL2_ttf 开发包并确保 pkg-config 可用。)
  endif
endif

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(EXTRA_CXXFLAGS) $(SRC) -o $@ $(LDLIBS)

.PHONY: run clean
run: $(TARGET)
	./$(TARGET)

clean:
	rm -f path_planning_sim path_planning_sim.exe
