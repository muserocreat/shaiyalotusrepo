#include <iostream>
#include <fstream>
int main() { std::ofstream file("C:\\ShaiyaServer\\PSM_Client\\Bin\\Data\\DropChanged.ini", std::ios_base::out | std::ios_base::app); if(!file) std::cout << "Cannot open\n"; else file << "Test from C++\n"; return 0; }
