# GoldRush 2.0 C++ 选手 Makefile
# make        -> 编译出 player.so
# make sim    -> 编译模拟器
# make clean  -> 清理产物

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -march=native -fPIC -Wall
TARGET   = player.so
SRC      = player.cpp

all: $(TARGET)

$(TARGET): $(SRC) game_api.h
	$(CXX) $(CXXFLAGS) -shared -o $(TARGET) $(SRC)

# ─── 模拟器 ───
SIM_TARGET = sim
SIM_SRCS   = main_sim.cpp simulator.cpp factors.cpp backtest.cpp
SIM_HDRS   = simulator.h game_api.h factors.h backtest.h

$(SIM_TARGET): $(SIM_SRCS) $(SIM_HDRS)
	$(CXX) $(CXXFLAGS) -o $(SIM_TARGET) $(SIM_SRCS)

clean:
	rm -f $(TARGET) $(SIM_TARGET)

.PHONY: all clean