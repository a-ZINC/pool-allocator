#include<iostream>
#include "../include/pool-allocator.hpp"


struct Person {
    std::string name;
    int age;

    Person(std::string name, int age) : name(name), age(age) {
        std::cout<<"Constructed " << name << std::endl;
    }
    ~Person() {
        std::cout<<"Destroyed " << name << std::endl;
    }

};

int main() {
    PoolAllocator<Person, 2> allocator;
    
    std::cout<< "\n--- Construct p1 ---" << std::endl;
    Person* p1 = allocator.construct("John", 30);

    std::cout<< "\n--- Construct p2 ---" << std::endl;
    Person* p2 = allocator.construct("Jane", 25);

    std::cout<< "\n--- Construct p3 ---" << std::endl;
    Person* p3 = allocator.construct("Bob", 40);

    allocator.debug_state();
    allocator.debug_freelist();

    std::cout<< "\n--- Access objects ---" << std::endl;
    if (p1) std::cout<< p1->name << " is " << p1->age << " years old" << std::endl;
    if (p2) std::cout<< p2->name << " is " << p2->age << " years old" << std::endl;
    if (p3) std::cout<< p3->name << " is " << p3->age << " years old" << std::endl;

    allocator.destroy(p1);
    allocator.deallocate(p1);

    allocator.destroy(p2);
    allocator.deallocate(p2);

    return 0;
}