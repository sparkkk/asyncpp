#include <cstdio>
#include <thread>
#include <atomic>
#include <vector>
#include <array>

#include <asyncpp/adv_semaphore.hpp>
#include <asyncpp/adv_queue.hpp>
#include <asyncpp/basic_semaphore.hpp>
#include <asyncpp/basic_queue.hpp>
#include <asyncpp/sync_queue.hpp>
#include <asyncpp/barrier.hpp>
#include <asyncpp/pthread_wrapper.hpp>
#include <atomic>


void test_basic_queue(int pc, int cc) {
    asyncpp::basic_queue<uint32_t> queue;
    queue.enable(100);
    std::atomic<uint32_t> counter = 0;
    auto t0 = std::chrono::steady_clock::now();
    auto producer_proc = [&](int n) {
        auto res = asyncpp::result_code::SUCCEED;
        for (int i = 0; i < 10000; ++i) {
            uint32_t value = counter++;
            //printf("producer1: pushing %d\n", value);
            if ((res = queue.push(value)) != asyncpp::result_code::SUCCEED) {
                //printf("producer%d break %d\n", n, res);
                break;
            }
            
            //printf("producer%d pushed %u\n", n, value);
        }
    };
    auto consumer_proc = [&](int n) {
        auto res = asyncpp::result_code::SUCCEED;
        while (true) {
            uint32_t value = 0;
            //printf("consumer1: popping\n");
            if ((res = queue.pop(value)) != asyncpp::result_code::SUCCEED) {
                //printf("consumer%d break %d\n", n, res);
                break;
            }
            //printf("consumer%d popped %u\n", n, value);
        }
    };
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    for (int k = 0; k < pc; ++k) {
        producers.emplace_back(producer_proc, k);
    }
    for (int k = 0; k < cc; ++k) {
        consumers.emplace_back(consumer_proc, k);
    }
    for (auto & p : producers) {
        p.join();
    }
    //printf("draining\n");
    //queue.drain();
    //printf("disabling\n");
    queue.disable();
    for (auto & c : consumers) {
        c.join();
    }
    //printf("joined\n");
    auto d = std::chrono::steady_clock::now() - t0;
    printf("pc=%d cc=%d cost=%ldms\n", pc, cc,
        std::chrono::duration_cast<std::chrono::milliseconds>(d).count()
    );
}

void test_queue(int pc, int cc) {
    asyncpp::adv_queue<uint32_t> queue;
    queue.enable(100);
    std::atomic<uint32_t> counter = 0;
    auto t0 = std::chrono::steady_clock::now();
    auto producer_proc = [&](int n) {
        for (int i = 0; i < 10000; ++i) {
            uint32_t value = counter++;
            //printf("producer%d: pushing %d\n", n, value);
            if (queue.push(value) != asyncpp::result_code::SUCCEED) {
                //printf("producer%d error\n", n);
                break;
            }
            //printf("producer%d pushed %u\n", n, value);
        }
    };
    auto consumer_proc = [&](int n) {
        while (true) {
            uint32_t value = 0;
            //printf("consumer%d: popping\n", n);
            if (queue.pop(value) != asyncpp::result_code::SUCCEED) {
                //printf("consumer%d break\n", n);
                break;
            }
            //printf("consumer%d popped %u\n", n, value);
        }
    };
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    for (int k = 0; k < pc; ++k) {
        producers.emplace_back(producer_proc, k);
    }
    for (int k = 0; k < cc; ++k) {
        consumers.emplace_back(consumer_proc, k);
    }
    for (auto & p : producers) {
        p.join();
    }
    //printf("draining\n");
    queue.drain();
    //printf("disabling\n");
    queue.disable();
    for (auto & c : consumers) {
        c.join();
    }
    //printf("joined\n");
    auto d = std::chrono::steady_clock::now() - t0;
    printf("pc=%d cc=%d cost=%ldms\n", pc, cc,
        std::chrono::duration_cast<std::chrono::milliseconds>(d).count()
    );

}

void test_fill_and_drain() {
    asyncpp::adv_queue<int> queue;
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
        printf("filled: %u\n", queue.get_size());
        printf("draining\n");
        queue.drain();
        printf("drained: %u\n", queue.get_size());
    }
    queue.unblock_pushing();
    printf("joining\n");
    producer.join();
    consumer.join();
    printf("end\n");
}

void test_capacity_change() {
    asyncpp::adv_queue<int> queue;
    queue.enable(5);
    auto producer = std::thread([&]() {
        int value = 0;
        while (true) {
            if (queue.push(value) != asyncpp::result_code::SUCCEED) {
                break;
            }
            //printf("producer: pushed %d\n", value);
            ++value;
        }
    });
    auto consumer = std::thread([&]() {
        int value = 0;
        while (true) {
            if (queue.pop(value) != asyncpp::result_code::SUCCEED) {
                break;
            }
            //printf("consumer: popped %d\n", value);
        }
    });
    queue.fill();
    printf("filled: %u/%u\n", queue.get_size(), queue.get_capacity());
    queue.drain();
    printf("drained: %u/%u\n", queue.get_size(), queue.get_capacity());
    printf("enlarge capacity\n");
    queue.change_capacity(20);
    queue.fill();
    printf("filled: %u/%u\n", queue.get_size(), queue.get_capacity());
    queue.drain();
    printf("drained: %u/%u\n", queue.get_size(), queue.get_capacity());
    printf("shrink capacity\n");
    queue.change_capacity(7);
    queue.fill();
    printf("filled: %u/%u\n", queue.get_size(), queue.get_capacity());
    queue.drain();
    printf("drained: %u/%u\n", queue.get_size(), queue.get_capacity());
    printf("quitting\n");
    queue.disable();
    producer.join();
    consumer.join();
}

void test_nonblock_and_timeout() {
    {
        asyncpp::adv_semaphore<> sem;
        asyncpp::result_code res;
        sem.set_value(1);
        sem.enable();
        res = sem.acquire();
        printf("res=%d\n", res);
        res = sem.try_acquire();
        printf("res=%d\n", res);
        auto t0 = std::chrono::steady_clock::now();
        res = sem.acquire(nullptr, std::chrono::seconds(3));
        auto dur = std::chrono::steady_clock::now() - t0;
        printf("res=%d dur=%ldms\n", res, std::chrono::duration_cast<milliseconds>(dur).count());
        sem.disable();
    }

    {
        asyncpp::result_code res;
        asyncpp::adv_queue<int> queue;
        queue.enable(1);
        res = queue.push(1);
        printf("res=%d\n", res);
        res = queue.try_push(1);
        printf("res=%d\n", res);
        auto t0 = std::chrono::steady_clock::now();
        res = queue.push(1, std::chrono::seconds(3));
        auto dur = std::chrono::steady_clock::now() - t0;
        printf("res=%d dur=%ldms\n", res, std::chrono::duration_cast<milliseconds>(dur).count());
        queue.disable();
    }
}

void test_sync_queue() {
    asyncpp::sync_queue<int> queue;
    queue.enable();
    auto producer = std::thread([&]() {
        for (int i = 0; i < 50; ++i) {
            queue.push(i);
            printf("producer: pushed %d\n", i);
        }
        queue.disable();
    });
    auto consumer = std::thread([&]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            int value;
            if (queue.pop(value) != asyncpp::result_code::SUCCEED) {
                printf("consumer break\n");
                break;
            }
            printf("consumer: popped %d\n", value);
        }
    });
    producer.join();
    consumer.join();
    printf("both joined\n");
}

void test_barrier() {
    uint32_t count = 5;
    std::vector<std::thread> threads;
    asyncpp::barrier<> barrier;
    barrier.enable(count);
    for (int i = 0; i < count; ++i) {
        threads.emplace_back([&barrier, i]() {
            asyncpp::result_code res = asyncpp::SUCCEED;
            for (int j = 0; j < 10; ++j) {
                if ((res = barrier.await()) != asyncpp::SUCCEED) {
                    printf("%d error\n", i);
                    break;
                }
                printf("%d passed\n", i);
            }
        });
    }
    for (auto & thread : threads) {
        thread.join();
    }
    barrier.disable();
    printf("end\n");
}

void test_thread_prio()
{
    std::atomic<bool> exit = false;
    auto proc = [&](int n, int p) {
        if (!asyncpp::this_thread::make_fifo(p)) {
            return;
        }
        uint64_t count = 0;
        while (!exit) {
            ++count;
        }
        printf("thread%d: %lu\n", n, count);
    };
    std::vector<std::thread> threads;
    threads.emplace_back(proc, 0, 1);
    threads.emplace_back(proc, 1, 99);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    exit = true;
    for (auto & t : threads) {
        t.join();
    }
    printf("done\n");
}

/*
template<typename ..._Ts>
struct Sizeof;
template<typename _T, typename ..._Ts>
struct Sizeof<_T, _Ts...> : public Sizeof<_Ts...>
{
    static constexpr size_t value = Sizeof<_Ts...>::value + 1;
};
template<>
struct Sizeof
{
    static constexpr size_t value = 0;
};*/

template<typename ...T>
struct Sizeof;
template<typename T, typename ...Ts>
struct Sizeof<T, Ts...> : public Sizeof<Ts...>
{
    static constexpr int value = Sizeof<Ts...>::value + 1;
};
template<>
struct Sizeof<>
{
    static constexpr int value = 0;
};

template <size_t ... _Values>
struct TestRange
{
    void print() {
        (printf("%lu ", _Values), ...);
        printf("\n");
    }
};


template<size_t ... _Values>
void print2(const asyncpp::Seq<_Values...> &) {
    (printf("%lu ", _Values), ...);
    printf("\n");
}



void test_inter_proc();
int main(int argc, const char * argv[])
{
    printf("%d %d\n", Sizeof<int, bool>::value, Sizeof<>::value);
    asyncpp::Range<TestRange, 2, 5> r;
    r.print();
    print2(asyncpp::Range2<2, 8>::seq{});
    //test_inter_proc();
    //test_sync_queue();
    //test_nonblock_and_timeout();
    //test_fill_and_drain();
    //test_capacity_change();
    //test_barrier();
    /*while (true) {
        test_queue(2, 1);
    }*/
    //test_thread_prio();
    return 0;
}

