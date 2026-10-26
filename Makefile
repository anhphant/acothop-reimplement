CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2
BASE := src/not-simple-2-tuneopt
BUILD := build

.PHONY: build clean exp-%

## Build base solver
build:
	$(CXX) $(CXXFLAGS) $(BASE)/main.cpp -o $(BASE)/main.exe

## Build a variant into build/<name> (overlay variant files onto base, then compile)
## Usage: make exp-change-score-top-K
exp-%:
	rm -rf $(BUILD)/$*
	mkdir -p $(BUILD)/$*
	cp -r $(BASE)/. $(BUILD)/$*/
	cp -r experiments/$*/. $(BUILD)/$*/ 2>/dev/null || true
	cd $(BUILD)/$* && $(CXX) $(CXXFLAGS) main.cpp -o main.exe

clean:
	rm -rf $(BUILD) $(BASE)/main.exe
