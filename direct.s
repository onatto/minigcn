# s_load_dwordx2  s[0:1], s[0:1], 0x0
# s_waitcnt       vmcnt(15) & lgkmcnt(0)
v_mov_b32       v1, s0
v_mov_b32       v2, s1
v_mad_u32_u24   v3, 64, s2, v0
v_mad_u32_u24   v1, v3, 4, v1
s_waitcnt       vmcnt(0) & expcnt(0) & lgkmcnt(0)
flat_store_dword v[1:2], v3
s_waitcnt       vmcnt(0) & expcnt(0) & lgkmcnt(0)
s_endpgm
