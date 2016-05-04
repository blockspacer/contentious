
#ifndef CONT_VECTOR_H
#define CONT_VECTOR_H

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>

#include <boost/thread/latch.hpp>

#include "bp_vector.h"


template <typename T>
class splt_vector;
template <typename T>
class cont_vector;


namespace contentious
{
    template <typename T>
    void foreach_splt(cont_vector<T> &cont,
                      const size_t a, const size_t b, const T val)
    {
        //chrono::time_point<chrono::system_clock> splt_start, splt_end;
        //splt_start = chrono::system_clock::now();

        splt_vector<T> splt = cont.detach();
        /* TODO: iterators, or at least all leaves at a time
        auto it_chunk_begin = splt_ret.begin() + chunk_sz * (i);
        auto it_chunk_end = splt_ret.begin() + chunk_sz * (i+1);
        for (auto it = it_chunk_begin; it != it_chunk_end; ++it) {
            splt_ret.mut_comp(*it, );
        }
        */
        for (size_t i = a; i < b; ++i) {
            splt.mut_comp(i, val);
        }
        cont.join(splt);
    }

    template <typename T>
    void foreach_splt_cvec(cont_vector<T> &cont,
                           const size_t a, const size_t b,
                           const cont_vector<T> &other)
    {
        //chrono::time_point<chrono::system_clock> splt_start, splt_end;
        //splt_start = chrono::system_clock::now();

        splt_vector<T> splt = cont.detach();
        /* TODO: iterators, or at least all leaves at a time
        auto it_chunk_begin = splt_ret.begin() + chunk_sz * (i);
        auto it_chunk_end = splt_ret.begin() + chunk_sz * (i+1);
        for (auto it = it_chunk_begin; it != it_chunk_end; ++it) {
            splt_ret.mut_comp(*it, );
        }
        */
        for (size_t i = a; i < b; ++i) {
            splt.mut_comp(i, other[i]);
        }
        cont.join(splt);
    }

    template <typename T>
    void reduce_splt(cont_vector<T> &cont, size_t a, size_t b)
    {
        //chrono::time_point<chrono::system_clock> splt_start, splt_end;
        //splt_start = chrono::system_clock::now();

        splt_vector<T> splt = cont.detach();
        for (size_t i = a; i < b; ++i) {
            // TODO: right now, reduces happen at index 0, which probably isn't
            // exactly right
            splt.mut_comp(0, splt._data[i]);
        }

        //splt_end = chrono::system_clock::now();
        //chrono::duration<double> splt_dur = splt_end - splt_start;
        //cout << "splt took: " << splt_dur.count() << " seconds; " << endl;
        //std::cout << "one cont_inc done: " << splt_ret._data[0] << std::endl;

        cont.join(splt);
    }

}   // end namespace contentious


template <typename T>
class Operator
{
public:
    T identity;
    Operator(T identity_in)
      : identity(identity_in) {}
    virtual T f(const T &lhs, const T &rhs) const = 0;
    virtual T inv(const T &lhs, const T &rhs) const = 0;
    virtual ~Operator() {}
};

template <typename T>
class Plus : public Operator<T>
{
public:
    Plus()
      : Operator<T>(0) {}
    inline T f(const T &lhs, const T &rhs) const
    {
        return lhs + rhs;
    }
    inline T inv(const T &lhs, const T &rhs) const
    {
        return lhs - rhs;
    }
    virtual ~Plus() {}
};

template <typename T>
class Multiply : public Operator<T>
{
public:
    Multiply()
      : Operator<T>(1) {}
    inline T f(const T &lhs, const T &rhs) const
    {
        return lhs * rhs;
    }
    inline T inv(const T &lhs, const T &rhs) const
    {
        return lhs / rhs;
    }
    virtual ~Multiply() {}
};


template <typename T>
class splt_vector
{
private:

public:
    tr_vector<T> _data;
    const Operator<T> *op;

    splt_vector(const cont_vector<T> &trunk)
      : _data(trunk._data.new_id()), op(trunk.op)
    {   /* nothing to do here */ }

    inline splt_vector<T> comp(const size_t i, const T &val)
    {
        _data.mut_set(i, op->f(_data.at(i), val));
        return *this;
    }

    inline void mut_comp(const size_t i, const T &val)
    {
        _data.mut_set(i, op->f(_data[i], val));
    }

};


template <typename T>
class cont_vector
{
    friend class splt_vector<T>;
    friend void contentious::foreach_splt<T>(
            cont_vector<T> &, const size_t, const size_t, const T);
    friend void contentious::foreach_splt_cvec<T>(
            cont_vector<T> &, const size_t, const size_t,
            const cont_vector<T> &);
    friend void contentious::reduce_splt<T>(
            cont_vector<T> &, size_t, size_t);

private:
    // TODO: timestamps for reading and writing?
    // if read <= write:
    //     no dependents
    // if read > write:
    //     register dependents
    //std::atomic<uint16_t> ts_r;
    //std::atomic<uint16_t> ts_w;

    tr_vector<T> _data;

    std::set<uint16_t> splinters;
    std::map<uint16_t, bool> resolved;

    std::mutex data_lock;
    boost::latch resolve_latch;

    cont_vector<T> *next;
    std::map<std::thread::id, bool> prescient;

    const Operator<T> *op;

public:
    cont_vector() = delete;
    cont_vector(Operator<T> *_op)
      : resolve_latch(4), next(nullptr), op(_op) {}

    cont_vector(const cont_vector<T> &other)
      : _data(other._data.new_id()), resolved(other.resolved),
        resolve_latch(4), next(nullptr), op(other.op)
    {
        // nothing to do here
    }

    cont_vector(const std::vector<T> &other)
      : _data(other), resolve_latch(4), next(nullptr), op(nullptr) {}

    inline const T &operator[](size_t i) const {    return _data[i]; }
    inline T &operator[](size_t i) {                return _data[i]; }

    inline const T &at(size_t i) const {    return _data.at(i); }
    inline T &at(size_t i) {                return _data.at(i); }

    inline const T &at_prescient(size_t i) const {  return next->at(i); }
    inline T &at_prescient(size_t i) {              return next->at(i); }

    inline size_t size() const {  return _data.size(); }

    // internal set passthroughs
    inline void unprotected_set(const size_t i, const T &val)
    {
        _data.mut_set(i, val);
    }
    inline void unprotected_push_back(const T &val)
    {
        _data.mut_push_back(val);
    }


    // user can tick the read counter
    //void tick_r() { ++tick_r; }

    splt_vector<T> detach()
    {
        splt_vector<T> ret(*this);
        {   // locked
            std::lock_guard<std::mutex> lock(this->data_lock);
            splinters.insert(ret._data.get_id());
        }
        return ret;
    }

    /*
    void cont_inc(cont_vector<double> &cont_ret,
            vector<double>::const_iterator a, vector<double>::const_iterator b)
    {
        chrono::time_point<chrono::system_clock> splt_start, splt_end;
        splt_vector<double> splt_ret = cont_ret.detach();
        double temp(0);
        /splt_start = chrono::system_clock::now();
        for (auto it = a; it != b; ++it) {
            temp += *it;
        }
        splt_ret.mut_comp(0, temp);
        cont_ret.join(splt_ret);
    }
    split_vector<T> map(function f)
    {
        cont_vector<double> cont_ret(new Plus<double>());
        std::vector<thread> cont_threads;
        int num_threads = thread::hardware_concurrency();
        size_t chunk_sz = test_vec.size()/num_threads;
        for (int i = 0; i < num_threads; ++i) {
            cont_threads.push_back(
                    thread(cont_inc,
                        std::ref(cont_ret),
                        test_vec.begin() + chunk_sz * i,
                        test_vec.begin() + chunk_sz * (i+1)));
        }
        for (int i = 0; i < num_threads; ++i) {
            cont_threads[i].join();
        }
        return cont_ret.at_prescient(0);
    }
    */
    void join(splt_vector<T> &splinter)
    {
        //--num_detached;
        // TODO: no check that other was actually detached from this
        // TODO: different behavior for other. don't want to loop over all
        // values for changes, there's too many and it ruins the complexity
        // TODO: generalize beyond addition, ya silly goose! don't forget
        // non-modification operations, like insert/remove/push_back/pop_back
        T diff;

        // if we haven't yet started building a more current vector, make one
        // grow out from ourselves. this is safe to modify, but this isn't,
        // because others may be depending on it for resolution
        {   // locked
            std::lock_guard<std::mutex> lock(this->data_lock);
            if (next == nullptr) {
                //std::cout << "making new next: " << std::endl;
                next = new cont_vector<T>(*this);
            }
        }

        const uint16_t sid = splinter._data.get_id();

        /*
        std::cout << "sid: " << sid << std::endl;
        for (auto it = splinters.begin(); it != splinters.end(); ++it) {
            std::cout << "splinters[i]: " << *it << std::endl;
        }
        */

        // TODO: better (lock-free) mechanism here
        if (splinters.find(sid) != splinters.end()) {
            for (size_t i = 0; i < next->size(); ++i) { // locked
                std::lock_guard<std::mutex> lock(this->data_lock);
                diff = op->inv(splinter._data[i], _data[i]);
                //std::cout << "resolving with diff " << diff << " at " << i
                //          << ", next->_data has size " << next->_data.size() << std::endl;
                next->_data = next->_data.set(i, op->f(next->at(i), diff));
            }
        }

        //splinters.erase(sid);
        resolve_latch.count_down();

    }

    inline cont_vector<T> pull()
    {
        resolve_latch.wait();
        return *next;
        //delete other.op;
    }

    inline void sync()
    {
        join();
        pull();
    }

    void resolve()
    {
        if (resolve_latch.try_wait()) {
            cont_vector<T> *forward = this->next;
            while (forward != nullptr) {
                for (size_t i = 0; i < next->size(); ++i) { // locked
                    std::lock_guard<std::mutex> lock(forward->data_lock);
                    T diff = forward->op->inv(forward->next->at(i), forward->at(i));
                    forward->next->_data = next->_data.set(
                                i,
                                forward->next->op->f(forward->next->at(i), diff));
                }
                forward = forward->next;
            }
        }
    }


    cont_vector<T> reduce(Operator<T> *op)
    {
        {   // locked
            std::lock_guard<std::mutex> lock(data_lock);
            this->op = op;
            if (next == nullptr) {
                //std::cout << "making new next: " << std::endl;
                next = new cont_vector<T>(op);
                next->unprotected_push_back(op->identity);
            }
        }
        std::vector<std::thread> cont_threads;
        int num_threads = std::thread::hardware_concurrency(); // * 16;
        size_t chunk_sz = this->size()/num_threads + 1;
        for (int i = 0; i < num_threads; ++i) {
            cont_threads.push_back(
              std::thread(contentious::reduce_splt<T>,
                     std::ref(*this),
                     chunk_sz * i,
                     std::min(chunk_sz * (i+1), this->size())
              )
            );
        }
        for (int i = 0; i < num_threads; ++i) {
            cont_threads[i].join();
        }
        return *next;
    }

    cont_vector<T> foreach(const Operator<T> *op, const T val)
    {
        {   // locked
            std::lock_guard<std::mutex> lock(data_lock);
            this->op = op;
            if (next == nullptr) {
                //std::cout << "making new next: " << std::endl;
                next = new cont_vector<T>(*this);
            }
        }
        std::vector<std::thread> cont_threads;
        int num_threads = std::thread::hardware_concurrency(); // * 16;
        size_t chunk_sz = (this->size()/num_threads) + 1;
        for (int i = 0; i < num_threads; ++i) {
            cont_threads.push_back(
              std::thread(contentious::foreach_splt<T>,
                     std::ref(*this),
                     chunk_sz * i,
                     std::min(chunk_sz * (i+1), this->size()),
                     val
              )
            );
        }
        for (int i = 0; i < num_threads; ++i) {
            cont_threads[i].join();
        }
        return *next;
    }

    // TODO: right now, this->()size must be equal to other.size()
    cont_vector<T> foreach(const Operator<T> *op, const cont_vector<T> &other)
    {
        {   // locked
            std::lock_guard<std::mutex> lock(data_lock);
            this->op = op;
            if (next == nullptr) {
                //std::cout << "making new next: " << std::endl;
                next = new cont_vector<T>(*this);
            }
        }
        std::vector<std::thread> cont_threads;
        int num_threads = std::thread::hardware_concurrency(); // * 16;
        size_t chunk_sz = (this->size()/num_threads) + 1;
        for (int i = 0; i < num_threads; ++i) {
            cont_threads.push_back(
              std::thread(contentious::foreach_splt_cvec<T>,
                     std::ref(*this),
                     chunk_sz * i,
                     std::min(chunk_sz * (i+1), this->size()),
                     other
              )
            );
        }
        for (int i = 0; i < num_threads; ++i) {
            cont_threads[i].join();
        }
        return *next;
    }


    cont_vector<T> stencil(const std::vector<int> &offs,
                           const std::vector<T> &coeffs,
                           const Operator<T> *op1 = new Multiply<T>,
                           const Operator<T> *op2 = new Plus<T>())
    {
        // get unique coefficients
        auto coeffs_unique = coeffs;
        auto it = std::unique(coeffs_unique.begin(), coeffs_unique.end());
        coeffs_unique.resize(std::distance(coeffs_unique.begin(), it));

        // perform coefficient multiplications on original vector
        // TODO: limit range of multiplications to only those necessary
        std::map<T, cont_vector<T>> step1;
        for (size_t i = 0; i < coeffs_unique.size(); ++i) {
            step1.emplace(std::make_pair(
                            coeffs_unique[i],
                            this->foreach(op1, coeffs_unique[i])
                         ));
            next = nullptr;
            assert(resolve_latch.try_wait());
            resolve_latch.reset(std::thread::hardware_concurrency());
        }
        std::cout << "after foreaches (this): " << *this << std::endl;

        {   // locked
            std::lock_guard<std::mutex> lock(data_lock);
            this->op = op;
            if (next == nullptr) {
                std::cout << "making new next..." << std::endl;
                next = new cont_vector<T>(*this);
            }
        }
        this->op = op2;
        next->op = op2;
        std::cout << "after foreaches (next): " << *this << std::endl;

        resolve_latch.reset(offs.size());
        // sum up the different parts of the stencil
        std::vector<splt_vector<T>> splts;
        for (size_t i = 0; i < offs.size(); ++i) {
            size_t start = 0;
            size_t end = this->size();
            if (offs[i] < 0) { start -= offs[i]; }
            else if (offs[i] > 0) { end -= offs[i]; }

            splts.push_back(this->detach());
            splt_vector<T> &splt = splts[splts.size()-1];
            splt.op = new Plus<T>();
            for (size_t j = start; j < end; ++j) {
                splt.mut_comp(j, step1.at(coeffs[i])[j+offs[i]]);
            }
            std::cout << "after sums: " << std::endl
                      << *this << std::endl;
        }
        for (size_t i = 0; i < offs.size(); ++i) {
            this->join(splts[i]);
        }
        assert(resolve_latch.try_wait());

        /*
        std::vector<std::thread> cont_threads;
        int num_threads = std::thread::hardware_concurrency(); // * 16;
        size_t chunk_sz = (this->size()/num_threads) + 1;
        for (int i = 0; i < num_threads; ++i) {
            cont_threads.push_back(
              std::thread(contentious::foreach_splt_cvec<T>,
                     std::ref(*this),
                     chunk_sz * i,
                     std::min(chunk_sz * (i+1), this->size()),
                     other
              )
            );
        }
        for (int i = 0; i < num_threads; ++i) {
            cont_threads[i].join();
        }
        */
        return *next;
    }


    void print_unresolved_info()
    {
        /*
        for (auto i: unresolved) {
            std::cout << "index " << i.first << " changed by " << unresolved[i.first].size()
                      << " parties" << std::endl;
            for (T j: i.second) {
                std::cout << j << " ";
            }
            std::cout << std::endl;
        }
        */
    }

    friend std::ostream &operator<<(std::ostream &out,
                                    const cont_vector<T> &cont)
    {
        out << "cont_vector{" << std::endl;
        out << "  data: " << cont._data << std::endl;
        out << "  splt: ";
        for (const auto &i : cont.splinters) { out << i << " "; }
        out << std::endl << "}/cont_vector" << std::endl;
        return out;
    }

};


#endif  // CONT_VECTOR_H

