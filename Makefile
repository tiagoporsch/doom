TARGET:=doom

CXX:=g++
CXXFLAGS:=-Iinclude -Wall -Wextra -g
CXXLIBS:=-lSDL2

SOURCES:=$(wildcard src/*.cc)
OBJECTS:=$(patsubst src/%.cc,build/%.o,$(SOURCES))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(CXXLIBS)

build/%.o: src/%.cc
	@mkdir -p "$(@D)"
	$(CXX) $(CXXFLAGS) -MMD -o $@ -c $<

run: $(TARGET)
	./$(TARGET)

debug: $(TARGET)
	gdb ./$(TARGET)

clean:
	rm -rf build/ $(TARGET)

-include $(OBJECTS:.o=.d)