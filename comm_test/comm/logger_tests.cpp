#include <comm/log/logger.h>
#include <comm/commassert.h>
#include <comm/str.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

/*
    Tests of the logger's hand-off from the threads that log to the writer thread.

    Several threads log tagged lines into a logger with its own file. The file has to hold every
    line exactly once, and the lines of each thread in the order that thread logged them.

    flush() waits until the writer thread has taken every queued message; the last one may still
    be being written when it returns, so the file is read again for a short while until it is
    complete.

    Like ref_tests, these assert with RASSERT, which is live in every configuration.
*/

namespace {

struct log_file_check
{
    unsigned found = 0;
    unsigned out_of_order = 0;
    std::vector<unsigned> next_expected;
};

/// @brief Reads the log file and checks the tagged lines: "logger_tests thread <t> line <n>"
log_file_check read_log_file(const char* path, unsigned thread_count)
{
    log_file_check check;
    check.next_expected.assign(thread_count, 0);

    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        const size_t at = line.find("logger_tests thread ");
        if (at == std::string::npos) {
            continue;
        }

        std::istringstream words(line.substr(at));
        std::string tag, thread_word, line_word;
        unsigned t = 0, n = 0;
        if (!(words >> tag >> thread_word >> t >> line_word >> n) || t >= thread_count) {
            continue;
        }

        if (n != check.next_expected[t]) {
            ++check.out_of_order;
        }
        check.next_expected[t] = n + 1;
        ++check.found;
    }
    return check;
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
void logger_tests()
{
    constexpr unsigned thread_count = 4;
    constexpr unsigned per_thread = 2000;
    constexpr unsigned total = thread_count * per_thread;

    char path[] = "logger_tests.log";
    std::remove(path);

    {
        coid::logger lg(false, true);
        lg.open(path);

        std::vector<std::thread> threads;
        for (unsigned t = 0; t < thread_count; ++t) {
            threads.emplace_back([&lg, t] {
                for (unsigned n = 0; n < per_thread; ++n) {
                    coid::logmsg_ref msg = lg.create_msg(coid::log::level::info, coid::log::target::primary_log, coid::tokenhash(), nullptr);
                    if (msg) {
                        msg->str() << "logger_tests thread " << t << " line " << n;
                    }
                }   // the message is queued when its last reference goes
            });
        }
        for (std::thread& t : threads) {
            t.join();
        }

        lg.flush();
    }

    // the writer may still be writing the last message flush() saw taken
    log_file_check check;
    for (int attempt = 0; attempt < 100; ++attempt) {
        check = read_log_file(path, thread_count);
        if (check.found >= total) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::remove(path);

    RASSERT(check.found == total);
    RASSERT(check.out_of_order == 0);
    for (unsigned t = 0; t < thread_count; ++t) {
        RASSERT(check.next_expected[t] == per_thread);
    }
}
