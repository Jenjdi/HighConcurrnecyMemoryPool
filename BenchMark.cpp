// #include"ConcurrentAlloc.h"
// void BenchMarkMalloc(size_t ntimes, size_t nthreads, size_t rounds)
//{
//     std::vector<std::thread> vthread(nthreads);
//     std::atomic<size_t> malloc_cost_time=0;
//     std::atomic<size_t> free_cost_time=0;
//     for (size_t i = 0; i < nthreads; i++) {
//         vthread[i] = std::thread([&]() {
//             std::vector<void*> ptrs;
//             ptrs.reserve(ntimes);
//             for (size_t k = 0; k < rounds; k++) {
//                 size_t begin1 = clock();
//                 for (size_t j = 0; j < ntimes; j++) {
//                     ptrs.push_back(malloc(16));
//                 }
//                 size_t end1 = clock();
//                 size_t begin2 = clock();
//                 for (size_t j = 0; j < ntimes; j++) {
//                     free(ptrs[j]);
//                 }
//                 size_t end2 = clock();
//                 ptrs.clear();
//                 malloc_cost_time += (end1 - begin1);
//                 free_cost_time += (end2 - begin2);
//             }
//         });
//     }
//     for (auto& threads : vthread)
//     {
//         threads.join();
//     }
//     std::cout << "--------------------------------" << std::endl;
//     std::cout << "malloc cost time:" << malloc_cost_time << "ms";
//     std::cout << std::endl;
//     std::cout << "free cost time:" << free_cost_time << "ms";
//     std::cout << std::endl;
//     std::cout << "total cost time:" << malloc_cost_time + free_cost_time << "ms" << std::endl;
//     std::cout << std::endl;
//     std::cout << "--------------------------------" << std::endl;
// }
// void BenchMarkTCMalloc(size_t ntimes, size_t nthreads, size_t rounds)
//{
//     std::vector<std::thread> vthread(nthreads);
//     std::atomic<size_t> malloc_cost_time=0;
//     std::atomic<size_t> free_cost_time=0;
//     for (size_t i = 0; i < nthreads; i++) {
//         vthread[i] = std::thread([&]() {
//             std::vector<void*> ptrs;
//             ptrs.reserve(ntimes);
//             for (size_t k = 0; k < rounds; k++) {
//                 size_t begin1 = clock();
//                 for (size_t j = 0; j < ntimes; j++) {
//                     ptrs.push_back(ConcurrentAlloc(16));
//                 }
//                 size_t end1 = clock();
//                 size_t begin2 = clock();
//                 for (size_t j = 0; j < ntimes; j++) {
//                     ConcurrentFree(ptrs[j]);
//                 }
//                 size_t end2 = clock();
//                 ptrs.clear();
//                 malloc_cost_time += (end1 - begin1);
//                 free_cost_time += (end2 - begin2);
//             }
//         });
//     }
//     for (auto& threads : vthread) {
//         threads.join();
//     }
//     std::cout << "--------------------------------" << std::endl;
//     std::cout << "tcmalloc cost time:" << malloc_cost_time << "ms";
//     std::cout << std::endl;
//     std::cout << "tcfree cost time:" << free_cost_time << "ms";
//     std::cout << std::endl;
//     std::cout << "total cost time:" << malloc_cost_time + free_cost_time << "ms" << std::endl;
//     std::cout << std::endl;
//     std::cout << "--------------------------------" << std::endl;
// }
// int main()
//{
//     size_t times = 100000;
//     size_t threads = 8;
//     size_t rounds = 50;
//     BenchMarkTCMalloc(times, threads, rounds);
//     std::cout << std::endl;
//     BenchMarkMalloc(times, threads, rounds);
//     
//     
//     
//
// }
#include "ConcurrentAlloc.h"
#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

// 随机大小可选（用于 stress 测试）
size_t GetRandomSize()
{
    static thread_local std::mt19937 gen(std::random_device {}());
    static thread_local std::uniform_int_distribution<size_t> dist(1, 8192); // 1 ~ 8KB
    return dist(gen);
}

void BenchMarkAllocator(size_t ntimes, size_t nthreads, size_t rounds, bool useTCMalloc, bool useRandomSize)
{
    vector<thread> vthread(nthreads);
    vector<size_t> malloc_time(nthreads);
    vector<size_t> free_time(nthreads);

    for (size_t i = 0; i < nthreads; i++) {
        vthread[i] = thread([&, i]() {
            vector<void*> ptrs;
            ptrs.reserve(ntimes);

            size_t local_malloc = 0, local_free = 0;

            for (size_t r = 0; r < rounds; r++) {
                // malloc
                auto start1 = high_resolution_clock::now();
                for (size_t j = 0; j < ntimes; j++) {
                    size_t size = useRandomSize ? GetRandomSize() : 16;
                    if (useTCMalloc)
                        ptrs.push_back(ConcurrentAlloc(size));
                    else
                        ptrs.push_back(malloc(size));
                }
                auto end1 = high_resolution_clock::now();

                // free
                auto start2 = high_resolution_clock::now();
                for (size_t j = 0; j < ntimes; j++) {
                    if (useTCMalloc)
                        ConcurrentFree(ptrs[j]);
                    else
                        free(ptrs[j]);
                }
                auto end2 = high_resolution_clock::now();

                local_malloc += duration_cast<milliseconds>(end1 - start1).count();
                local_free += duration_cast<milliseconds>(end2 - start2).count();
                ptrs.clear();
            }

            malloc_time[i] = local_malloc;
            free_time[i] = local_free;
        });
    }

    for (auto& t : vthread)
        t.join();

    size_t total_malloc = 0, total_free = 0;
    for (size_t i = 0; i < nthreads; ++i) {
        total_malloc += malloc_time[i];
        total_free += free_time[i];
    }

    string tag = useTCMalloc ? "TCMalloc" : "malloc";
    string sizeTag = useRandomSize ? "[Random Size]" : "[Fixed Size=16]";

    cout << "============ " << tag << " " << sizeTag << " ============" << endl;
    cout << nthreads << " threads * " << rounds << " rounds * " << ntimes << " allocs" << endl;
    cout << "Malloc time: " << total_malloc << " ms" << endl;
    cout << "Free time:   " << total_free << " ms" << endl;
    cout << "Total time:  " << (total_malloc + total_free) << " ms" << endl;
    cout << "==========================================" << endl
         << endl;
}

int main()
{
    size_t ntimes = 100000;
    size_t threads = 8;
    size_t rounds = 5;

    // 固定大小 vs 随机大小都测试一次
    BenchMarkAllocator(ntimes, threads, rounds, true, false); // TCMalloc 固定大小
    BenchMarkAllocator(ntimes, threads, rounds, false, false); // malloc 固定大小

    //BenchMarkAllocator(ntimes, threads, rounds, true, true); // TCMalloc 随机大小
    //BenchMarkAllocator(ntimes, threads, rounds, false, true); // malloc 随机大小

    return 0;
}
