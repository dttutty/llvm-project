#include <iostream>

void example() {
    for (int i = 0; i < 1024; ++i) {
        std::cout << "Iteration " << i << std::endl;
    }
}

int main() {
    example();
    return 0;
}
