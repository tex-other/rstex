g++ -o cpf CreatePoolFile/CreatePoolFile.cpp
./cpf TEX_STRING rstex.cpp.pre rstex.h.pre
g++ -o rstex -O3 -std=c++14 -Wno-unused-result -Wno-dangling-else rstex.cpp
