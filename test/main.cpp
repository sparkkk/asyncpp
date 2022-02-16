#include <cstdio>
#include <thread>
#include <atomic>

#include <asyncpp/semaphore.hpp>
#include <asyncpp/queue.hpp>

void test_basic() {
    asyncpp::queue<int> queue;
    queue.enable(20);
    auto producer = std::thread([&]() {
        for (int i = 0; i < 100; ++i) {
            queue.push(i);
            printf("producer: pushed %d\n", i);
        }
    });
    auto consumer = std::thread([&]() {
        int value = 0;
        for (int i = 0; i < 100; ++i) {
            queue.pop(value);
            printf("consumer: popped %d\n", value);
        }
    });
    printf("joining\n");
    producer.join();
    consumer.join();
    printf("end\n");
}

void test_fill_and_drain() {
    asyncpp::queue<int> queue;
    queue.enable(20);
    queue.suspend_pushing();
    auto producer = std::thread([&]() {
        for (int i = 0; i < 100; ++i) {
            queue.push(i);
            printf("producer: pushed %d\n", i);
        }
    });
    auto consumer = std::thread([&]() {
        int value = 0;
        for (int i = 0; i < 100; ++i) {
            queue.pop(value);
            printf("consumer: popped %d\n", value);
        }
    });
    for (int i = 0; i < 5; ++i) {
        queue.fill();
        queue.drain();
    }
    queue.continue_pushing();
    printf("joining\n");
    producer.join();
    consumer.join();
    printf("end\n");
}

int main(int argc, const char * argv[])
{
    test_fill_and_drain();
    return 0;
}

