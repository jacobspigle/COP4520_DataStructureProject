#include <iostream>

void print_bits(uint64_t data)
{
    uint64_t print_mask = (uint64_t) 0b1 << 63;

    for(int i=0; i<64; i++) {
        if(print_mask & data) {
            std::cout << "1";
        }
        else {
            std::cout << "0";
        }

        print_mask >>= 1;
    }

    std::cout << std::endl;
}

int main(void)
{
    int initial_data = 29;

    std::cout << "Initial data: " << initial_data << std::endl;
    std::cout << std::endl;

    int *initial_pointer = (int *) malloc(sizeof(int));
    *initial_pointer = initial_data;

    uint64_t mask = (uint64_t) 0b11 << 62;

    std::cout << "Initial pointer bits:" << std::endl;
    print_bits((uint64_t) initial_pointer);
    std::cout << std::endl;

    std::cout << "Mask bits:" << std::endl;
    print_bits(mask);
    std::cout << std::endl;

    // clear the 2 MSBs
    int *clean_pointer = (int *) ((uint64_t) initial_pointer & (~mask));

    // set tag
    int *packed_pointer = (int *) ((uint64_t) clean_pointer | mask);

    std::cout << "Cleaned pointer bits:" << std::endl;
    print_bits((uint64_t) clean_pointer);
    std::cout << std::endl;
    
    std::cout << "Packed bits:" << std::endl;
    print_bits((uint64_t) packed_pointer);
    std::cout << std::endl;

    int *unpacked_pointer = (int *) ((uint64_t) packed_pointer & (~mask));

    std::cout << "Unpacked pointer bits:" << std::endl;
    print_bits((uint64_t) unpacked_pointer);
    std::cout << std::endl;

    int recovered_data = *unpacked_pointer;

    std::cout << "Recovered data: " << recovered_data << std::endl;
}