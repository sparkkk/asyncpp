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
    queue.block_pushing();
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
        printf("filling\n");
        queue.fill();
        printf("draining\n");
        queue.drain();
    }
    queue.continue_pushing();
    printf("joining\n");
    producer.join();
    consumer.join();
    printf("end\n");
}

void test_capacity_change() {
    asyncpp::queue<int> queue;
    queue.enable(5);
    auto producer = std::thread([&]() {
        int value = 0;
        while (true) {
            if (queue.push(value) != asyncpp::result_code::SUCCEED) {
                break;
            }
            printf("producer: pushed %d\n", value);
            ++value;
        }
    });
    auto consumer = std::thread([&]() {
        int value = 0;
        while (true) {
            if (queue.pop(value) != asyncpp::result_code::SUCCEED) {
                break;
            }
            printf("consumer: popped %d\n", value);
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    printf("enlarge capacity\n");
    queue.change_capacity(20);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    printf("shrink capacity\n");
    queue.change_capacity(5);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    printf("quitting\n");
    queue.disable();
    producer.join();
    consumer.join();
}

void test_instant_and_timeout() {
    asyncpp::semaphore<> sem;
    asyncpp::result_code res;
    sem.enable(1);
    res = sem.acquire(1);
    printf("res=%d\n", res);
    res = sem.nonblock_acquire(1);
    printf("res=%d\n", res);
    res = sem.timeout_acquire(1, std::chrono::seconds(1));
    printf("res=%d\n", res);
    sem.disable();

    
    asyncpp::queue<int> queue;
    queue.enable(1);
    res = queue.push(1);
    printf("res=%d\n", res);
    res = queue.instant_push(1);
    printf("res=%d\n", res);
    res = queue.timeout_push(1, std::chrono::seconds(1));
    printf("res=%d\n", res);
    queue.disable();
}

void test_peek() {
    asyncpp::queue<int> queue;
    queue.enable(20);
    auto producer = std::thread([&]() {
        for (int i = 0; i < 100; ++i) {
            if (queue.push(i) != asyncpp::result_code::SUCCEED) {
                break;
            }
            printf("producer: pushed %d\n", i);
        }
    });
    auto consumer = std::thread([&]() {
        int value = 0;
        for (int i = 0; i < 100; ++i) {
            if (queue.peek(value) != asyncpp::result_code::SUCCEED) {
                break;
            }
            printf("consumer: peeked %d\n", value);
            if (queue.pop(value) != asyncpp::result_code::SUCCEED) {
                break;
            }
        }
    });
    printf("joining\n");
    producer.join();
    consumer.join();
    printf("end\n");
}

int main(int argc, const char * argv[])
{
    //test_instant_and_timeout();
    //test_peek();
    test_instant_and_timeout();
    return 0;
}

