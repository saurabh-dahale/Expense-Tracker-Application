# Makefile for the C++ expense tracker.
#
#   make           build both targets
#   make test      build and run the unit tests
#   make clean     remove build output
#
# -std=c++17 is the floor agreed on Day 1 so the project builds on older Apple
# clang and on lab machines. -Wall -Wextra are on because the type-system
# section of the report cites compiler diagnostics as evidence, which only
# works if they are enabled.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -pthread
LDFLAGS  ?= -pthread

BIN      := expense_tracker
TESTBIN  := run_tests
HEADERS  := expense.h format.h storage.h queries.h summary.h

.PHONY: all test clean

all: $(BIN) $(TESTBIN)

$(BIN): main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ main.cpp $(LDFLAGS)

$(TESTBIN): tests.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $@ tests.cpp $(LDFLAGS)

test: $(TESTBIN)
	./$(TESTBIN)

clean:
	rm -f $(BIN) $(TESTBIN)
