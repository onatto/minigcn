# Minimal GCN

Minimal example testbed of running bare-metal GCN assembly using HSA runtime.

If you want to run own generated code, a simple trick is to use `S_SETPC_B64` to jump to your code instead of implementing the codeobj format the HSA expects.

uncpp'fied from [here](https://github.com/ROCm-Developer-Tools/LLVM-AMDGPU-Assembler-Extra/tree/master/examples/asm-kernel).

### Dependencies

- [ROCm](https://github.com/RadeonOpenCompute/ROCm)

- [LLVM7](https://releases.llvm.org/download.html#7.0.0) to compile the assembly (optional, precompiled asm included).

### Links

- [VEGA ISA](https://developer.amd.com/wp-content/resources/Vega_Shader_ISA_28July2017.pdf)

- [HSA specs](http://www.hsafoundation.com/standards/) (specifically the runtime)

- [AMD Kernel Code Object Format](https://rocm-documentation.readthedocs.io/en/latest/ROCm_Compiler_SDK/ROCm-Codeobj-format.html)

- [Art of AMDGCN Assembly](https://gpuopen.com/amdgcn-assembly/)

### Why

Because GCN is *awesome*. Thank you AMD for making such an amazing architecture and its ISA and software open-source.
