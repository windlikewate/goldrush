CXX      = g++
CXXFLAGS = -std=c++17 -O2 -march=native -Wall
INCLUDE  = -I../src

TARGET   = sim
SRCS     = main_sim.cpp simulator.cpp factors.cpp factors_enhanced.cpp backtest.cpp
HDRS     = simulator.h factors.h factors_enhanced.h backtest.h ../src/game_api.h

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: all clean