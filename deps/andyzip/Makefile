
# note: this makefile is purely for the examples and tests
# there is no need to build the library itself.


CCFLAGS = -I include
CC = clang

ifeq ($(OS),Windows_NT)
  # note that this makefile assumes you have basic unix utilities such as unxutils
  CCFLAGS += -fms-compatibility-version=19
  CC = "C:\Program Files\LLVM\bin\clang.exe"
  EXE = .exe
else
  CCFLAGS += -g -O2 -lstdc++
endif

# add your binary here
BINARIES = \
	bin/andyzip$(EXE)

all: $(BINARIES)

clean:
	rm -f $(BINARIES)

bin/andyzip$(EXE): examples/andyzip.cpp include/andyzip/decoder.hpp 
	$(CC) $(CCFLAGS) $< -o $@



