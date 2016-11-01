#include "contentious.h"

namespace contentious {

void threadpool::finish()
{
    std::unique_lock<std::mutex> lk(fin_m);
    bool done = false;
    while (!done) {
        done = fin_cv.wait_for(lk, std::chrono::milliseconds{1}, [this] {
            bool ret = true;
            for (int p = 0; p < HWCONC; ++p) {
                ret &= (tasks[p].isEmpty() && resns[p].isEmpty());
            }
            return ret;
        });
    }
}

void threadpool::worker(int p)
{
    closure *task;
    closure *resn;
    for (;;) {
        // wait until we have something to do
        sems[p].wait();
        // threadpool is done
        if (!spin) {
            assert(tasks[p].isEmpty());
            assert(resns[p].isEmpty());
            break;
        }
        if (tasks[p].isEmpty()) {
            // must resolve
            resn = resns[p].frontPtr();
            assert(resn);
#ifdef CTTS_TIMING
            std::chrono::time_point<std::chrono::system_clock> splt_start, splt_end;
            splt_start = std::chrono::system_clock::now();
#endif
            (*resn)();
#ifdef CTTS_TIMING
            splt_end = std::chrono::system_clock::now();
            rslv_durs[p].add(splt_start, splt_end);
#endif
            resns[p].popFront();
            fin_cv.notify_one();
        } else {
            // normal parallel processing
            task = tasks[p].frontPtr();
            assert(task);
            (*task)();
            tasks[p].popFront();
        }
    }
}

}
