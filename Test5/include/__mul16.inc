__MUL16:
  push b
  mov b, h
  mov c, l
  lxi h, 0
  mvi a, 17
__MUL16_1:
  dcr a
  jz __MUL16_4
  dad h
  xchg
  jnc __MUL16_2
  dad h
  inx h
  jmp __MUL16_3
__MUL16_2:
  dad h
__MUL16_3:
  xchg
  jnc __MUL16_1
  dad b
  jnc __MUL16_1
  inx d
  jmp __MUL16_1
  ret
__MUL16_4:
  pop b
  ret
