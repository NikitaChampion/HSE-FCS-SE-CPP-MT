#include <iostream>
#include <optional>
#include <utility>
#include <thread>
#include <chrono>
#include <random>
#include <fstream>
#include <vector>
#include <mutex>
#include "omp.h"

//#define GENERATE
//#define FILE_OUT

constexpr int64_t MIN_VALUE = 1000;
constexpr int64_t MAX_VALUE = 999'999'999ll;

std::mutex mut;

bool Read(int *n, int64_t *l, int64_t *r, int *thread_number, std::string *file_name) {  // NOLINT
#ifdef GENERATE
    std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist_n(2, 9);
    *n = dist_n(gen);
    std::uniform_int_distribution<int64_t> dist_border(MIN_VALUE, MAX_VALUE);
    *l = dist_border(gen);
    *r = dist_border(gen);
    if (l > r) {
        std::swap(l, r);
    }
    std::uniform_int_distribution<int> dist_t(1, 10000);
    *thread_number = dist_t(gen);
    *file_name = "answer.txt";
    std::cout << "Number n: " << *n << '\n';
    std::cout << "Your borders: [l, r] = [" << *l << ", " << r << "]\n";
    std::cout << "Amount of threads: " << *thread_number << '\n';
#else
    std::cout << "Input your number n (in (1, 10)): ";
    std::cin >> *n;
    if (*n <= 1 || *n >= 10) {
        std::cout << "Incorrect number!\n";
        return false;
    }
    std::cout << "Input your left border (in [" << MIN_VALUE << ", " << MAX_VALUE << "]): ";
    std::cin >> *l;
    if (*l < MIN_VALUE || *l > MAX_VALUE) {
        std::cout << "Incorrect l!\n";
        return false;
    }
    std::cout << "Input your right border (in [" << *l << ", " << MAX_VALUE << "]): ";
    std::cin >> *r;
    if (*r < *l || *r > MAX_VALUE) {
        std::cout << "Incorrect r!\n";
        return false;
    }
    // see https://livebook.manning.com/book/c-plus-plus-concurrency-in-action/chapter-2/92 for details
    unsigned int hardware_threads = std::thread::hardware_concurrency();
    std::cout << "Optimal number of threads for your machine: " << (hardware_threads != 0 ? hardware_threads : 2)
              << '\n';
    std::cout << "Input amount of threads (in [1, 10000]): ";
    std::cin >> *thread_number;
    if (*thread_number < 1 || *thread_number > 10000) {
        std::cout << "Incorrect amount of threads!\n";
        return false;
    }
#endif
#ifdef FILE_OUT
    std::cout << "Input name of output file: ";
    std::cin >> *file_name;
    std::ofstream out(*file_name);
    if (!out.is_open()) {
        std::cout << "Incorrect file name!\n";
        out.close();
        return false;
    }
    out.close();
#endif
    return true;
}

unsigned int Fun(int64_t x) {
    unsigned int y = 0; // [0, 1024)
    while (x != 0) {
        y |= 1u << unsigned(x % 10);
        x /= 10;
    }
    return y;
}

struct Info {
    int64_t number;
    int thread_id;
};

void Compute(int n, int64_t from, int64_t to, std::vector<Info> &numbers) {
#pragma omp parallel for default(none) shared(numbers, mut, from, to, n)
    for (int64_t i = from; i <= to; ++i) {
        if (Fun(i) == Fun(i * n)) {
            mut.lock();
            numbers.push_back({i, omp_get_thread_num()});
            mut.unlock();
        }
    }
}

void Print(int n, int l, int r, int threads_num, const std::string &file_name, const std::vector<Info> &answer) {
#ifdef FILE_OUT
    std::ofstream out(file_name);
    out << "Number n: " << n << '\n';
    out << "Your borders: [l, r] = [" << l << ", " << r << "]\n";
    out << "Amount of threads: " << threads_num << '\n';
    out << "Size of result array: " << answer.size() << '\n';
    for (const Info &info : answer) {
        out << "original: " << info.number << ", modified: " << info.number * n <<
            ", thread: " << info.thread_id << '\n';
    }
    out.close();
#else
    std::cout << "Size of result array: " << answer.size() << '\n';
    for (const Info &info : answer) {
        std::cout << "original: " << info.number << ", result: " << info.number * n <<
                  ", thread: " << info.thread_id << '\n';
    }
#endif
}

int main() {
    std::ios_base::sync_with_stdio(false);
    int n, thread_number;
    int64_t l, r;
    std::string file_name;
    if (!Read(&n, &l, &r, &thread_number, &file_name)) {
        return 1;
    }
    omp_set_dynamic(0);      // запретить библиотеке openmp менять число потоков во время исполнения
    omp_set_num_threads(thread_number); // установить число потоков в thread_number

    std::vector<Info> numbers;
    auto begin = std::chrono::steady_clock::now();

    Compute(n, l, r, numbers);

    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

    Print(n, l, r, thread_number, file_name, numbers);
    std::cout << "Time (computation): " << elapsed_ms.count() / 1000. << " sec\n";
    return 0;
}
