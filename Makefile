CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
TARGET = market_maker_sim
SRCDIR = .
SOURCES = $(SRCDIR)/underlying.cpp $(SRCDIR)/option.cpp $(SRCDIR)/market_maker.cpp $(SRCDIR)/main.cpp
OBJECTS = $(SOURCES:.cpp=.o)
HEADERS = $(SRCDIR)/types.hpp $(SRCDIR)/underlying.hpp $(SRCDIR)/option.hpp $(SRCDIR)/position.hpp $(SRCDIR)/base_market_maker.hpp $(SRCDIR)/market_maker.hpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

underlying.o: underlying.cpp underlying.hpp types.hpp
option.o: option.cpp option.hpp types.hpp underlying.hpp
market_maker.o: market_maker.cpp market_maker.hpp base_market_maker.hpp position.hpp option.hpp underlying.hpp types.hpp
main.o: main.cpp market_maker.hpp base_market_maker.hpp position.hpp option.hpp underlying.hpp types.hpp