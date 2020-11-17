#include <iostream>
#include <optional>
#include <thread>
#include <chrono>
#include <random>
#include <fstream>
#include <list>
#include <vector>
#include <mutex>

//#define GENERATE
#define FILE_OUT

constexpr int64_t MIN_VALUE = 1000;
constexpr int64_t MAX_VALUE = 999'999'999ll;

std::mutex mut;

bool Read(int *n, int *thread_number, std::string *file_name) { // NOLINT
#ifdef GENERATE
    std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist_n(2, 9);
    *n = dist_n(gen);
    std::uniform_int_distribution<int64_t> dist_t(1, MAX_VALUE - MIN_VALUE + 1);
    *thread_number = dist_t(gen);
    *file_name = "answer.txt";
    std::cout << "Number n: " << *n << '\n';
    std::cout << "Amount of threads: " << *thread_number << '\n';
#else
    std::cout << "Input your number n (in (1, 10)): ";
    std::cin >> *n;
    if (*n <= 1 || *n >= 10) {
        std::cout << "Incorrect number!\n";
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

void ComputeThread(int n, int64_t from, int64_t to, std::list<int64_t> &numbers) {
    for (int64_t i = from; i <= to; ++i) {
        if (Fun(i) == Fun(i * n)) {
            mut.lock();
            numbers.push_back(i);
            mut.unlock();
        }
    }
}

void Compute(int n, int64_t thread_number, std::list<int64_t> &numbers) {
    int64_t loop_size = (MAX_VALUE - MIN_VALUE + 1) / thread_number;
    std::vector<std::thread> thr(thread_number);
    for (int64_t i = 0; i < thread_number; ++i) {
        if (i != thread_number - 1) {
            thr[i] = std::thread{ComputeThread, n, MIN_VALUE + loop_size * i, MIN_VALUE + loop_size * (i + 1) - 1,
                                 std::ref(numbers)};
        } else {
            thr[i] = std::thread{ComputeThread, n, MIN_VALUE + loop_size * i, MAX_VALUE, std::ref(numbers)};
        }
    }
    for (std::thread &Thread : thr) {
        Thread.join();
    }
}

void Print(int n, int threads_num, const std::string &file_name, const std::list<int64_t> &numbers) {
#ifdef FILE_OUT
    std::ofstream out(file_name);
    out << "Number n: " << n << '\n';
    out << "Amount of threads: " << threads_num << '\n';
    for (int64_t x : numbers) {
        out << x << '\n';
    }
    out.close();
#else
    for (int64_t x : numbers) {
        std::cout << x << '\n';
    }
#endif
}

int main() {
    std::ios_base::sync_with_stdio(false);
    int n, thread_number;
    std::string file_name;
    if (!Read(&n, &thread_number, &file_name)) {
        return 1;
    }

    std::list<int64_t> numbers;
    auto begin = std::chrono::steady_clock::now();

    Compute(n, thread_number, numbers);

    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

    Print(n, thread_number, file_name, numbers);
    std::cout << "Time (computation): " << elapsed_ms.count() / 1000. << " sec\n";
    return 0;
}