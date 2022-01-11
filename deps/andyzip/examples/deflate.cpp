
#include <cstdint>
#include <cstdio>
#include <vector>
#include <iostream>
#include <fstream>


#include <andyzip/deflate_encoder.hpp>

int main() {
  andyzip::deflate_encoder enc;

  //static const uint8_t text[] = "to be or not to be, that is the question! 1234 1234 1234";
  //static const uint8_t text[] = "123123123";

  std::ifstream in("C:\\Users\\Andy\\gilgamesh\\examples\\data\\4GRG.pdb", std::ios::binary);
  in.seekg(0, std::ios::end);
  size_t size = (size_t)in.tellg();
  std::vector<uint8_t> text(size);
  in.seekg(0, std::ios::beg);
  in.read((char*)text.data(), size);
  std::vector<uint8_t> buffer(size);
  enc.encode(buffer.data(), buffer.data() + size, text.data(), text.data() + size - 1);
}
