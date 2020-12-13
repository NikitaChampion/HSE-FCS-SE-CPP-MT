#ifdef __APPLE__

#include <dispatch/dispatch.h>

#else

#include <semaphore.h>

#endif

#include <iostream>
#include <optional>
#include <utility>
#include <thread>
#include <chrono>
#include <random>
#include <fstream>
#include <vector>
#include <functional>
#include <mutex>
#include <string>

//#define GENERATE
#define FILE_OUT

constexpr int NUM_SINGLE = 10;
constexpr int NUM_DOUBLE = 15;

struct rk_sema {
#ifdef __APPLE__
    dispatch_semaphore_t sem;
#else
    sem_t sem;
#endif
};

static inline void rk_sema_init(struct rk_sema *s, uint32_t value) {
#ifdef __APPLE__
    dispatch_semaphore_t *sem = &s->sem;

    *sem = dispatch_semaphore_create(value);
#else
    sem_init(&s->sem, 0, value);
#endif
}

static inline void rk_sema_wait(struct rk_sema *s) {
#ifdef __APPLE__
    dispatch_semaphore_wait(s->sem, DISPATCH_TIME_FOREVER);
#else
    int r;
    do {
        r = sem_wait(&s->sem);
    } while (r == -1 && errno == EINTR);
#endif
}

static inline void rk_sema_post(struct rk_sema *s) {
#ifdef __APPLE__
    dispatch_semaphore_signal(s->sem);
#else
    sem_post(&s->sem);
#endif
}

rk_sema write_semaphore; // Семафор

bool Read(int *thread_number, std::string *file_name) {  // NOLINT
#ifdef GENERATE
    std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int64_t> dist_t(1, MAX_VALUE - MIN_VALUE + 1);
    *thread_number = dist_t(gen);
    *file_name = "answer.txt";
    std::cout << "Amount of people: " << *thread_number << '\n';
#else
    std::cout << "Input amount of people (in [1, 10000]): ";
    std::cin >> *thread_number;
    if (*thread_number < 1 || *thread_number > 10000) {
        std::cout << "Incorrect amount of people!\n";
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
    rk_sema_init(&write_semaphore, 1);
    return true;
}

enum class Customer {
    NONE,
    MAN,
    WOMAN
};

void Initialize(std::vector<Customer> *single_rooms, std::vector<std::pair<Customer, Customer>> *double_rooms) {
    *single_rooms = std::vector<Customer>(NUM_SINGLE, Customer::NONE);
    *double_rooms = std::vector<std::pair<Customer, Customer>>(NUM_DOUBLE, {Customer::NONE, Customer::NONE});
}

void SpendTheNight(std::chrono::time_point<std::chrono::steady_clock> begin, Customer customer, int id, int &could_not,
                   std::vector<Customer> &single_rooms, std::vector<std::pair<Customer, Customer>> &double_rooms) {
    rk_sema_wait(&write_semaphore);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    std::string name = (customer == Customer::WOMAN ? "Lady" : "Gentleman");
    auto cur_time = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - begin);
    std::cout << "Time: " << elapsed_ms.count() << " ms, " << name << " #" << id << " came to your hotel\n";

    rk_sema_post(&write_semaphore);


    rk_sema_wait(&write_semaphore);

    for (size_t i = 0; i < double_rooms.size(); ++i) {
        if (double_rooms[i].first == Customer::NONE) {
            double_rooms[i].first = customer;
            cur_time = std::chrono::steady_clock::now();
            elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - begin);
            std::cout << "Time: " << elapsed_ms.count() << " ms, " << name << " #" << id
                      << " checked in to the double room #" << i << ", thread: " << std::this_thread::get_id() << '\n';
            rk_sema_post(&write_semaphore);
            return;
        } else if (double_rooms[i].first == customer && double_rooms[i].second == Customer::NONE) {
            double_rooms[i].second = customer;
            cur_time = std::chrono::steady_clock::now();
            elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - begin);
            std::cout << "Time: " << elapsed_ms.count() << " ms, " << name << " #" << id
                      << " checked in to the double room #" << i << ", thread: " << std::this_thread::get_id() << '\n';
            rk_sema_post(&write_semaphore);
            return;
        }
    }
    for (size_t i = 0; i < single_rooms.size(); ++i) {
        if (single_rooms[i] == Customer::NONE) {
            single_rooms[i] = customer;
            cur_time = std::chrono::steady_clock::now();
            elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - begin);
            std::cout << "Time: " << elapsed_ms.count() << " ms, " << name << " #" << id
                      << " checked in to the single room #" << i << ", thread: " << std::this_thread::get_id() << '\n';
            rk_sema_post(&write_semaphore);
            return;
        }
    }
    cur_time = std::chrono::steady_clock::now();
    elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur_time - begin);
    std::cout << "Time: " << elapsed_ms.count() << " ms, " << name << " #" << id
              << " could not find a room, thread: " << std::this_thread::get_id() << '\n';
    ++could_not;

    rk_sema_post(&write_semaphore);
}

int Compute(std::chrono::time_point<std::chrono::steady_clock> begin, int thread_number,
            std::vector<Customer> &single_rooms, std::vector<std::pair<Customer, Customer>> &double_rooms) {
    std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist_customer(0, 1);
    int id = 0, could_not = 0;
    std::vector<std::thread> threads(thread_number);
    for (int i = 0; i < thread_number; ++i) {
        Customer customer = dist_customer(gen) == 0 ? Customer::MAN : Customer::WOMAN;
        threads[i] = std::thread{SpendTheNight, begin, customer, id++, std::ref(could_not),
                                 std::ref(single_rooms), std::ref(double_rooms)};
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    for (std::thread &thr : threads) {
        thr.join();
    }
    return could_not;
}

void Print(int threads_num, const std::string &file_name, int could_not, const std::vector<Customer> &single_rooms,
           const std::vector<std::pair<Customer, Customer>> &double_rooms) {
    std::string to_write = "\nSingle rooms:\n";
    for (size_t i = 0; i < single_rooms.size(); ++i) {
        if (i == 0) {
            to_write += '|';
        }
        to_write += ' ' + std::to_string(i) + " |";
    }
    to_write += '\n';
    for (size_t i = 0; i < single_rooms.size(); ++i) {
        if (i == 0) {
            to_write += '|';
        }
        to_write += std::string(" ") + (single_rooms[i] == Customer::MAN ? 'M' :
                                        (single_rooms[i] == Customer::WOMAN ? 'W' : ' ')) +
                    std::string(" |");
    }
    to_write += "\n\nDouble rooms:\n";
    for (size_t i = 0; i < double_rooms.size(); ++i) {
        if (i == 0) {
            to_write += '|';
        }
        to_write += "  " + std::to_string(i);
        if (i < 10) {
            to_write += ' ';
        }
        to_write += " |";
    }
    to_write += '\n';
    for (size_t i = 0; i < double_rooms.size(); ++i) {
        if (i == 0) {
            to_write += '|';
        }
        to_write += std::string(" ") + (double_rooms[i].first == Customer::MAN ? 'M' :
                                        (double_rooms[i].first == Customer::WOMAN ? 'W' : ' ')) + ' ';
        to_write += (double_rooms[i].second == Customer::MAN ? 'M' :
                     (double_rooms[i].second == Customer::WOMAN ? 'W' : ' ')) + std::string(" |");
    }
    to_write += '\n';
    to_write += "Number of people that could not find rooms: " + std::to_string(could_not) + '\n';
    std::cout << to_write;
#ifdef FILE_OUT
    std::ofstream out(file_name);
    out << "Amount of people: " << threads_num << '\n';
    out << to_write;
    out.close();
#endif
}

int main() {
    std::ios_base::sync_with_stdio(false);
    std::string file_name;
    int could_not = 0, thread_number;
    if (!Read(&thread_number, &file_name)) {
        return 1;
    }
    std::vector<Customer> single_rooms;
    std::vector<std::pair<Customer, Customer>> double_rooms;
    Initialize(&single_rooms, &double_rooms);
    Print(thread_number, file_name, could_not, single_rooms, double_rooms);

    auto begin = std::chrono::steady_clock::now();

    could_not = Compute(begin, thread_number, single_rooms, double_rooms);

    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

    Print(thread_number, file_name, could_not, single_rooms, double_rooms);
    std::cout << "Time (spend): " << elapsed_ms.count() / 1000. << " sec\n";
    return 0;
}
