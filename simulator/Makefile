# GoldRush 2.0 模拟器 Makefile
CXX      = g++
CXXFLAGS = -std=c++17 -O2 -march=native -Wall
INCLUDE  = -I../src

TARGET   = sim
SRCS     = main_sim.cpp simulator.cpp factors.cpp backtest.cpp
HDRS     = simulator.h factors.h backtest.h ../src/game_api.h

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
