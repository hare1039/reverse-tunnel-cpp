CXX ?= clang++

.PHONY: release debug
release:
	mkdir -p build && \
    cd build       && \
    conan install .. --profile ../profiles/release && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build .

debug:
	mkdir -p build && \
    cd build       && \
    conan install .. --profile ../profiles/debug && \
    cmake .. -DCMAKE_BUILD_TYPE=Debug && \
    cmake --build .

clean:
	rm -rf build

lldb:
	$(CXX) *.cpp -glldb -o lldb -std=c++17 -fcoroutines-ts -lboost_program_options -lboost_system
