HSA_DIR = /opt/rocm
INC  = -I$(HSA_DIR)/include
LIBS = -L$(HSA_DIR)/lib -lhsa-runtime64

all:
	gcc host.c -g $(INC) $(LIBS) -o build/host
	cd build && ./host

compile-asm:
	clang -x assembler -target amdgcn--amdhsa -mcpu=polaris10 -c -o build/asm.o asm.s
	clang -target amdgcn--amdhsa build/asm.o -o build/asm.co
