#include "sock.hpp"

#include "monster_generated.h"

#include <iostream> // C++ header file for printing
#include <fstream> // C++ header file for file access

int main() {
    std::cout << "Hello, World!" << std::endl;
    std::ifstream infile;
    infile.open("monsterdata_test.mon", std::ios::binary | std::ios::in);
    if (!infile.is_open()) {
        std::cerr << "Failed to open file" << std::endl;
        return 1;
    }
    infile.seekg(0,std::ios::end);
    int length = infile.tellg();
    infile.seekg(0,std::ios::beg);
    char *data = new char[length];
    infile.read(data, length);
    infile.close();

    auto monster = MyGame::Sample::GetMonster(data);
    std::cout << "Monster HP: " << monster->hp() << std::endl;
    std::cout << "Monster Mana: " << monster->mana() << std::endl;
    std::cout << "Monster Name: " << monster->name()->c_str() << std::endl;
    std::cout << "Monster Inventory: " << std::endl;
    for (int i = 0; i < monster->inventory()->size(); i++) {
        std::cout << monster->inventory()->Get(i) << std::endl;
    }

    return 0;
}