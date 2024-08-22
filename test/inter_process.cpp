#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <unistd.h>

#include <stdio.h>

#include <asyncpp/adv_queue.hpp>
#include <asyncpp/flat_ring_queue.hpp>
#include <thread>
#include <chrono>

struct Shared
{
    Shared() {
        queue.enable(10);
    }
    uint32_t code = 0xABCD;
    asyncpp::adv_queue<int32_t, true, asyncpp::flat_ring_queue<int32_t, 11>> queue;
};

void parent_proc(Shared & shared) {
    printf("parent %d %p %x\n", getpid(), &shared, shared.code);
    for (int i = 0; i < 50; ++i) {
        shared.queue.push(i);
        printf("parent %d\n", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    shared.queue.drain();
    printf("parent drained\n");
    shared.queue.disable();
    printf("parent disabled\n");
}

void child_proc(Shared & shared) {
    printf("child %d %p %x\n", getpid(), &shared, shared.code);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    while (true) {
        int32_t v = -1;
        if (shared.queue.pop(v) != asyncpp::result_code::SUCCEED) {
            printf("child break\n");
            break;
        } 
        printf("child %d\n", v);
    }
}

void test_inter_proc()
{
    int fd = shm_open("test_shared_mem", O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
    if (fd == -1) {
        printf("shm_open failed\n");
        return;
    }
    ftruncate(fd, sizeof(Shared));
    void * ptr = mmap(NULL, sizeof(Shared), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == (void *)(-1)) {
        printf("mmap failed\n");
        return;
    }
    printf("ptr is %p\n", ptr);
    Shared * shared = new(ptr) Shared();
    pid_t pid = fork();
    if (pid == 0) {
        child_proc(*shared);
    } else if (pid > 0) {
        parent_proc(*shared);
    } else {
        printf("fork failed\n");
    }
    munmap(ptr, sizeof(Shared));
    close(fd);
}