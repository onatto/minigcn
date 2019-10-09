HSA_DIR=/opt/rocm
INC=-I$(HSA_DIR)/include
LIBS=-L$(HSA_DIR)/lib -lhsa-runtime64
FWARNS=-Wno-int-conversion -Wno-incompatible-pointer-types
.PHONY: build

all: compile-asm direct build run


build: 
	gcc host.c -g $(INC) $(LIBS) $(FWARNS)  -o build/host 

run:
	cd build && ./host

compile-asm:
	clang -x assembler -target amdgcn--amdhsa -mcpu=polaris10 -c -o build/asm.o asm.s
	clang -target amdgcn--amdhsa build/asm.o -o build/asm.co

direct:
	clrxasm --arch=GFX9 -b rawcode direct.s -o build/raw.gcn

