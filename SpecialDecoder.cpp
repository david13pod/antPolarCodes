F_function_vectorized(LLR[0][7].data(), LLR[0][6].data(), 64);
Repetition_vectorized(LLR[0][6].data(), Bits[0][6][0].data(), 64);
G_function_vectorized(LLR[0][7].data(), LLR[0][6].data(), Bits[0][6][0].data(), 64);
F_function_vectorized(LLR[0][6].data(), LLR[0][5].data(), 32);
Repetition_vectorized(LLR[0][5].data(), Bits[0][5][0].data(), 32);
G_function_vectorized(LLR[0][6].data(), LLR[0][5].data(), Bits[0][5][0].data(), 32);
F_function_vectorized(LLR[0][5].data(), LLR[0][4].data(), 16);
G_function_0R_vectorized(LLR[0][4].data(), LLR[0][3].data(), 8);
G_function_0R(LLR[0][3].data(), LLR[0][2].data(), 4);
SPC(LLR[0][2].data(), Bits[0][2][1].data(), 4);
Combine_0R(Bits[0][2][1].data(), Bits[0][3][1].data(), 4);
Combine_0R(Bits[0][3][1].data(), Bits[0][4][0].data(), 8);
G_function_vectorized(LLR[0][5].data(), LLR[0][4].data(), Bits[0][4][0].data(), 16);
F_function_vectorized(LLR[0][4].data(), LLR[0][3].data(), 8);
F_function(LLR[0][3].data(), LLR[0][2].data(), 4);
Repetition(LLR[0][2].data(), Bits[0][2][0].data(), 4);
G_function(LLR[0][3].data(), LLR[0][2].data(), Bits[0][2][0].data(), 4);
SPC(LLR[0][2].data(), Bits[0][2][1].data(), 4);
Combine(Bits[0][2][0].data(), Bits[0][2][1].data(), Bits[0][3][0].data(), 4);
G_function_vectorized(LLR[0][4].data(), LLR[0][3].data(), Bits[0][3][0].data(), 8);
SPC(LLR[0][3].data(), Bits[0][3][1].data(), 8);
Combine(Bits[0][3][0].data(), Bits[0][3][1].data(), Bits[0][4][1].data(), 8);
Combine(Bits[0][4][0].data(), Bits[0][4][1].data(), Bits[0][5][1].data(), 16);
Combine(Bits[0][5][0].data(), Bits[0][5][1].data(), Bits[0][6][1].data(), 32);
Combine(Bits[0][6][0].data(), Bits[0][6][1].data(), Bits[0][7][0].data(), 64);
