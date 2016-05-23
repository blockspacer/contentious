
#ifndef CONT_VECTOR_H
#define CONT_VECTOR_H

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include <boost/thread/latch.hpp>

#include "bp_vector.h"
#include "contentious.h"

template <typename T>
class splt_vector;
template <typename T>
class cont_vector;

using contentious::hwconc;

// locks in:
//   * move constructor (other)
//   * destructor (*this)
//   * freeze (*this)
//   * detach (*this, dep)
//   * reattach (*this, dep)
//   * resolve (dep)
//
//   _data/_used
//   splinters
//   resolved
//
// TODO
//   * multiple dependents (one->many and many->one)
//   * correctly resolve many->one value deps (non identity index deps)
//   * efficient mutable iterators for tr_vector
//   * lifetimes
//   * efficient/correct dependent vector shapes (reduce should be just single
//     val; not all deps are necessarily the same shape as their originators
//   * cheap attachment/resolution for identity index deps


template <typename T>
class splt_vector
{
    friend void contentious::reduce_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t);

    friend void contentious::foreach_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t, const T &);

    friend void contentious::foreach_splt_cvec<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t,
                    const std::reference_wrapper<cont_vector<T>> &);

    friend void contentious::foreach_splt_off<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t,
                    const std::reference_wrapper<cont_vector<T>> &,
                    const int &);

    friend class cont_vector<T>;

private:
    tr_vector<T> _data;
    const contentious::op<T> op;

public:
    splt_vector() = delete;

    splt_vector(const tr_vector<T> &_used, const contentious::op<T> &_op)
      : _data(_used.new_id()), op(_op)
    {   /* nothing to do here */ }

    inline splt_vector<T> comp(const size_t i, const T &val)
    {
        _data = _data.set(i, op.f(_data.at(i), val));
        return *this;
    }

    inline void mut_comp(const size_t i, const T &val)
    {
        _data.mut_set(i, op.f(_data[i], val));
    }

};

template <typename T>
class cont_vector
{
    friend void contentious::reduce_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t);

    friend void contentious::foreach_splt<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t, const T &);

    friend void contentious::foreach_splt_cvec<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t,
                    const std::reference_wrapper<cont_vector<T>> &);

    friend void contentious::foreach_splt_off<T>(
                    cont_vector<T> &, cont_vector<T> &,
                    const size_t, const size_t,
                    const std::reference_wrapper<cont_vector<T>> &,
                    const int &);

    friend class splt_vector<T>;

private:
    struct dependency_tracker
    {
        dependency_tracker()
          : indexmap(contentious::identity)
        {   /* nothing to do here! */ }
        dependency_tracker(const tr_vector<T> &_data)
          : _used(_data), indexmap(contentious::identity)
        {   /* nothing to do here! */ }
        dependency_tracker(const tr_vector<T> &_data,
                           const std::function<int(int)> imap,
                           const contentious::op<T> opin)
          : _used(_data), indexmap(imap), op(opin)
        {   /* nothing to do here! */ }

        const tr_vector<T> _used;
        std::function<int(int)> indexmap;
        contentious::op<T> op;

    };

    tr_vector<T> _data;
    std::mutex data_lock;
    contentious::op<T> op;

    // forward tracking : dep_ptr -> tracker
    std::map<const cont_vector<T> *, dependency_tracker> tracker;
    // resolving onto (backward) :  uid -> latch
    std::map<int32_t, std::unique_ptr<boost::latch>> resolve_latch;
    std::map<int32_t, bool> reattached;
    volatile bool unsplintered = true;

    //std::vector<cont_vector<T> *> dependents;

    template <typename... U>
    void exec_par(void f(cont_vector<T> &, cont_vector<T> &,
                         const size_t, const size_t, const U &...),
                  cont_vector<T> &dep, const U &... args);

public:
    cont_vector()
    {   /* nothing to do here */ }
    cont_vector(const contentious::op<T> _op)
      : op(_op)
    {   /* nothing to do here */ }

    cont_vector(const cont_vector<T> &other)
      : _data(other._data.new_id()), op(other.op)
    {   /* nothing to do here */ }
    cont_vector<T> &operator=(cont_vector<T> other)
    {
        _data = other._data.new_id();
        std::swap(op, other.op);
        return *this;
    }

    /*
    cont_vector(cont_vector<T> &&other)
      : _data(std::move(other._data)),
        _used(std::move(other._used)),
        splinters(std::move(other.splinters)),
        resolve_latch(0),
        dependents(std::move(other.dependents)),
        unsplintered(std::move(other.unsplintered)),
        op(std::move(other.op))
    {   // locked (TODO: this is really a terrible idea)
        std::lock_guard<std::mutex> lock(other.data_lock);

        resolved = (std::move(other.resolved));
        other.unsplintered = true;
        std::cout << "RPOBLESM" << std::endl;
    }
    */

    /*
    cont_vector(const std::vector<T> &other)
      : _data(other), op(nullptr)
    {   // nothing to do here
    }
    */

    ~cont_vector()
    {
        // finish this round of resolutions to avoid segfaulting on ourselves
        for (auto &rl : resolve_latch) {
            rl.second->wait();
        }
        /*
        // make sure splinters are reattached, to avoid segfaulting in the
        // cont_vector I depend on
        // TODO: BAD SPINLOCK! BAD!!!
        if (!unsplintered) {
            while (reattached.size() != hwconc) {
                std::this_thread::yield();
            }
        }
        bool flag = false;
        while (!flag) {
            std::this_thread::yield();
            flag = true;
            for (const auto &it : reattached) {
                std::lock_guard<std::mutex> lock(data_lock);
                flag &= it.second;
            }
        }
        */
        // should be okay now
    }

    inline size_t size() const  {  return _data.size(); }

    // internal passthroughs
    inline const T &operator[](size_t i) const  { return _data[i]; }
    inline T &operator[](size_t i)              { return _data[i]; }

    inline const T &at(size_t i) const  { return _data.at(i); }
    inline T &at(size_t i)              { return _data.at(i); }

    inline void unprotected_set(const size_t i, const T &val)
    {
        _data.mut_set(i, val);
    }
    inline void unprotected_push_back(const T &val)
    {
        _data.mut_push_back(val);
    }

    void freeze(cont_vector<T> &dep,
                bool nocopy = false, uint16_t splinters = hwconc,
                std::function<int(int)> imap = contentious::identity);
    splt_vector<T> detach(cont_vector &dep);
    void reattach(splt_vector<T> &splinter, cont_vector<T> &dep,
                  size_t a, size_t b);
    void resolve(cont_vector<T> &dep);

    cont_vector<T> reduce(const contentious::op<T> op);
    cont_vector<T> foreach(const contentious::op<T> op, const T &val);
    cont_vector<T> foreach(const contentious::op<T> op, cont_vector<T> &other);
    cont_vector<T> stencil(const std::vector<int> &offs,
                           const std::vector<T> &coeffs,
                           const contentious::op<T> op1 = contentious::mult,
                           const contentious::op<T> op2 = contentious::mult);

    friend std::ostream &operator<<(std::ostream &out,
                                    const cont_vector<T> &cont)
    {
        out << "cont_vector{" << std::endl;
        out << "  id: " << cont._data.get_id() << std::endl;
        out << "  data: " << cont._data << std::endl;
        //out << "  used: " << cont._used << std::endl;
        //out << "  splt: ";
        //for (const auto &i : cont.splinters) { out << i << " "; }
        out << std::endl << "}/cont_vector" << std::endl;
        return out;
    }

};

#include "cont_vector-impl.h"

#endif  // CONT_VECTOR_H

