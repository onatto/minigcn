.hsa_code_object_version 2,0
.hsa_code_object_isa 8, 0, 3, "AMD", "AMDGPU"

.text
.p2align 8
.amdgpu_hsa_kernel hello_world

hello_world:

   .amd_kernel_code_t
      enable_sgpr_kernarg_segment_ptr = 1
      enable_sgpr_workgroup_id_x = 1
      is_ptr64 = 1
      compute_pgm_rsrc1_vgprs = 8
      compute_pgm_rsrc1_sgprs = 8
      compute_pgm_rsrc2_user_sgpr = 2
      kernarg_segment_byte_size = 8
      wavefront_sgpr_count = 16
      workitem_vgpr_count = 16
  .end_amd_kernel_code_t

  // s2 = workgroup-id.x
  // v3 = global-id.x

  s_load_dwordx2 s[0:1], s[0:1] 0x0
  s_waitcnt lgkmcnt(0)
  v_mov_b32 v1, s0                    // set address base for memory
  v_mov_b32 v2, s1
  v_mad_u32_u24 v3, 64, s2, v0        // v3 = 64*workgroup-id.x + local-id.x
  v_mad_u32_u24 v1, v3, 4, v1         // v1 = v3*4 + v1
  s_waitcnt 0 
  flat_store_dword v[1:2], v3
  s_waitcnt 0
  s_endpgm
