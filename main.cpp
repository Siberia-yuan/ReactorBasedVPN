#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stack>
#include "Reactor.h"
#include <chrono>

int main(int, char**) {
    Reactor ra("10.0.4.13",8888);
    ra.start();
    return 0;
}
