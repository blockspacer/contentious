
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
#include <memory>
#include <limits>

#include <cassert>
#include <cmath>

#include "boost/coroutine/symmetric_coroutine.hpp"
#include "boost/coroutine/asymmetric_coroutine.hpp"
#include "boost/variant.hpp"
#include "boost/bind.hpp"

// redundant, but in case we reorganize testing...
#include "../bp_vector/cont_vector.h"

#include "bp_vector-tests.h"
#include "reduce-tests.h"
#include "cont_vector-tests.h"

using namespace std;

int test_simple()
{
    bp_vector<double> test = bp_vector<double>();
    int test_sz = 129;
    for (int i = 0; i < test_sz; ++i) {
        test.mut_push_back(1000 + test_sz - i);
    }
    for (int i = 0; i < test_sz; ++i) {
        if ((1000 + test_sz - i) != test[i]) {
            cerr << "! test_simple failed: at index " << i
                 << " got " << test[i]
                 << ", expected " << 1000 + test_sz - i << endl;
            return 1;
        }
    }
    for (int i = 0; i < test_sz; ++i) {
        test.at(i) = 1000 + i;
    }
    for (int i = 0; i < test_sz; ++i) {
        if ((1000 + i) != test[i]) {
            cerr << "! test_simple failed: at index " << i
                 << " got " << test[i]
                 << ", expected " << 1000 + i << endl;
            return 1;
        }
    }
    cerr << "+ test_simple passed" << endl;
    return 0;
}

int test_insert()
{
    // insert random reals into the trie and a reference vec
    random_device r;
    default_random_engine e_val(r());
    uniform_real_distribution<double> dist_real;
    auto rand_val = std::bind(dist_real, e_val);

    vector<double> test_vec;
    bp_vector<double> test_trie;
    size_t test_sz = 400000;
    for (size_t i = 0; i < test_sz; ++i) {
        double val = rand_val();
        test_vec.push_back(val);
        test_trie.mut_push_back(val);
    }
    // check random values
    //default_random_engine e_ind(r());
    //uniform_int_distribution<size_t> dist_int(0, test_sz);
    //auto rand_ind = std::bind(dist_int, e_ind);
    for (size_t i = 0; i < test_sz; ++i) {
        if ((test_vec.at(i) != test_trie.at(i))) {
            cerr << "test_insert failed: at index " << i
                 << "; got " << test_trie.at(i)
                 << ", expected " << test_vec.at(i) << endl;
            return 1;
        }
    }
    cerr << "+ test_insert passed" << endl;
    return 0;
}

int test_pers()
{
    vector<ps_vector<double>> test;
    int test_sz = 1025;
    test.push_back(ps_vector<double>());
    for (int i = 1; i < test_sz; ++i) {
        test.push_back(test[i-1].push_back(1000 + test_sz - i));
        //cout << i << "- " << test[i-1] << endl;
        //cout << i << "  " << test[i] << endl;
    }
    for (int i = 0; i < test_sz; ++i) {
        for (int j = i-1; j >= 0; --j) {
            test[i] = test[i].set(j, i);
        }
       //cout << i << "- " << test[i-2] << endl;
       //cout << i << "  " << test[i] << endl;
    }
    vector<double> ref;
    if (!test[0].empty()) {
        cerr << "! test_pers failed: first vector not empty" << endl;
        return 1;
    }
    //cout << "0 " << test[0] << endl;
    for (int i = 1; i < test_sz; ++i) {
        ref.push_back(1000 + test_sz - i);
        for (int j = 0; j < i; ++j) {
            if (i != test[i].at(j)) {
                cerr << "! test_pers failed: at copy " << i
                     << ", index " << j
                     << "; got " << test[i].at(j)
                     << ", expected " << i << endl;
                return 1;
            }

        }
       //cout << i << " " << test[i] << endl;
    }
    cerr << "+ test_pers passed" << endl;
    return 0;
}

int test_pers_alternate()
{
    ps_vector<double> test1;
    ps_vector<double> test2;
    for (int i = 0; i < 8; ++i) {
        test1 = test1.push_back(i);
    }
    test2 = test1.push_back(9);
    test1 = test1.push_back(8);
    //cout << test1 << endl;
    //cout << test2 << endl;

    vector<double> ref1;
    vector<double> ref2;
    for (int i = 0; i < 8; ++i) {
        ref1.push_back(i);
        ref2.push_back(i);
    }
    ref1.push_back(8);
    ref2.push_back(9);
    for (int i = 0; i < 9; ++i) {
        if (test1[i] != ref1[i]) {
            cerr << "! test_pers_alt failed at test1: at index " << i
                 << "; got " << test1[i]
                 << ", expected " << ref1[i] << endl;
            return 1;
        }
        if (test2[i] != ref2[i]) {
            cerr << "! test_pers_alt failed at test2: at index " << i
                 << "; got " << test2[i]
                 << ", expected " << ref2[i] << endl;
            return 1;
        }
    }

    ps_vector<double> test3;
    ps_vector<double> test4;
    for (int i = 0; i < 20; ++i) {
        test3 = test3.push_back(i);
    }
    test4 = test3.push_back(22);
    test3 = test3.push_back(21);
    //cout << test3 << endl;
    //cout << test4 << endl;

    vector<double> ref3;
    vector<double> ref4;
    for (int i = 0; i < 20; ++i) {
        ref3.push_back(i);
        ref4.push_back(i);
    }
    ref3.push_back(21);
    ref4.push_back(22);
    for (int i = 0; i < 21; ++i) {
        if (test3[i] != ref3[i]) {
            cerr << "! test_pers_alt failed at test3: at index " << i
                 << "; got " << test3[i]
                 << ", expected " << ref3[i] << endl;
            return 1;
        }
        if (test4[i] != ref4[i]) {
            cerr << "! test_pers_alt failed at test4: at index " << i
                 << "; got " << test4[i]
                 << ", expected " << ref4[i] << endl;
            return 1;
        }
    }

    cerr << "+ test_pers_alt passed" << endl;
    return 0;
}

int test_pers_iter()
{
    tr_vector<double> pers;
    int index;

    // testing ++
    for (int i = 0; i < 514; ++i) {
        //cout << "at i: " << i << endl;
        pers = tr_vector<double>();
        for (int j = 0; j < i; ++j) {
            pers = pers.push_back(i);
        }
        //cout << "let's iterate! ";
        index = 0;
        for (auto it = pers.begin(); it != pers.end(); ++it) {
            //cout << "at index: " << index << endl;
            if (*it != pers[index]) {
                cerr << "! test_pers_iter failed: at index " << index
                     << "; got " << *it
                     << ", expected " << pers[index] << endl;
                return 1;
            }
            //cout << *it << " ";
            ++index;
        }
        if (index != i) {
            cerr << "! test_pers_iter failed: iterated " << index
                 << " times, but its size was " << i << endl;
            return 1;
        }

    }

    // testing +
    random_device r;
    default_random_engine e_val(r());
    uniform_int_distribution<int> dist_int(0, 513);
    auto rand_val = std::bind(dist_int, e_val);
    for (int i = 0; i < 10000; ++i) {
        index = rand_val();
        //cout << index << endl;
        auto it_st = pers.begin() + index;
        auto it = it_st;
        if (*it != pers[index]) {
            cerr << "! test_pers_iter (+) failed: at index " << index
                << "; got " << *it
                << ", expected " << pers[index] << endl;
            return 1;
        }
    }

    cerr << "+ test_pers_iter passed" << endl;
    return 0;
}

int test_trans()
{
    vector<tr_vector<double>> transs;
    vector<ps_vector<double>> perss;
    int test_sz = 20;
    transs.push_back(tr_vector<double>());
    perss.push_back(ps_vector<double>());
    for (int i = 1; i < test_sz; ++i) {
        //cout << "round 1..." << endl;
        transs.push_back(transs[i-1].push_back(1000 + test_sz - i));
        //cout << i << "'s id: " << transs[i].get_id() << endl;
        perss.push_back(transs[i].make_persistent());
        perss[i] = perss[i].set(0, 666);
        perss[i] = perss[i].set(i-1, -1*(1000 + test_sz - i));
    }
    tr_vector<double> next = perss[test_sz-1].make_transient();
    //cout << "next is: " << next << endl;
    for (int i = 0; i < test_sz-1; ++i) {
        //next = next.set(i, 777);
        next.mut_set(i, 777);
    }
    //cout << "changed next: " << next << endl;

    vector<double> ref;
    if (!perss[0].empty() || !transs[0].empty()) {
        cerr << "! test_trans failed: first vector not empty" << endl;
        return 1;
    }
    //cout << "0 " << transs[0] << endl;
    //cout << "0 " << perss[0] << endl;
    for (int i = 1; i < test_sz; ++i) {
        ref.push_back(1000 + test_sz - i);
        //cout << i << " " << transs[i] << endl;
        //cout << i << " " << perss[i] << endl;
        for (int j = 0; j < i; ++j) {
            //cout << transs[i].at(j) << " "
            //     << perss[i].at(j)  << " "
            //     << ref.at(j)       << " : ";
            if (transs[i].at(j) != ref.at(j)) {
                cerr << "! test_trans failed: at tr_vector copy " << i
                     << ", index " << j
                     << "; got " << transs[i].at(j)
                     << ", expected " << ref.at(j) << endl;
                return 1;
            }
            double pers_val_test = ref.at(j);
            if (j == i-1) {
                pers_val_test *= -1;
            } else if (j == 0) {
                pers_val_test = 666;
            }
            if (perss[i].at(j) != pers_val_test) {
                cerr << "! test_trans failed: at ps_vector copy " << i
                     << ", index " << j
                     << "; got " << perss[i].at(j)
                     << ", expected " << pers_val_test << endl;
                return 1;
            }
        }
        //cout << endl;
    }
    for (int i = 0; i < test_sz-1; ++i) {
        if (next.at(i) != 777) {
            cerr << "! test_trans failed: problems with make_transient"
                 << "; at index i, got " << next.at(i)
                 << ", expected " << 777 << endl;
            return 1;
        }
    }

    cerr << "+ test_trans passed" << endl;
    return 0;
}

int test_make()
{
    ps_vector<double> ps1;
    auto tr1 = ps1.make_transient();
    auto tr2 = ps1.make_transient();
    auto tr3 = ps1.make_transient();
    auto ps2 = tr1.make_persistent();
    auto tr4 = ps2.make_transient();
    auto tr5 = ps2.make_transient();
    auto tr6 = ps2.make_transient();
    if (!(ps1.get_id() == 0 && ps2.get_id() == 0 &&
          tr1.get_id() < tr2.get_id() && tr2.get_id() < tr3.get_id() &&
          tr3.get_id() < tr4.get_id() && tr4.get_id() < tr5.get_id() &&
          tr5.get_id() < tr6.get_id())) {
        cerr << "! test_make failed: bad get_id()" << endl;
        return 1;
    }
    cerr << "+ test_make passed" << endl;
    return 0;
}

int test_coroutine()
{
    stringstream ss;
    const int num=5, width=15;
    boost::coroutines::asymmetric_coroutine<string>::push_type writer(
        [&](boost::coroutines::asymmetric_coroutine<string>::pull_type& in){
            // pull values from upstream, lay them out 'num' to a line
            for (;;){
                for(int i=0;i<num;++i){
                    // when we exhaust the input, stop
                    if(!in) return;
                    ss << std::setw(width) << in.get();
                    // now that we've handled this item, advance to next
                    in();
                }
                // after 'num' items, line break
                ss << endl;
            }
        });

    vector<string> words{ "peas", "porridge", "hot", "peas", "porridge",
                          "cold", "peas", "porridge", "in", "the",
                          "pot", "nine", "days", "old"
    };

    std::copy(boost::begin(words),boost::end(words),boost::begin(writer));
    string expected = \
"           peas       porridge            hot           peas       porridge\n"
"           cold           peas       porridge             in            the\n"
"            pot           nine           days            old";
    if (ss.str() != expected) {
        cerr << "! test_coroutine failed: wrong output" << endl;
        return 1;
    }
    cerr << "+ test_coroutine passed" << endl;
    return 0;
}


typedef boost::coroutines::symmetric_coroutine<std::vector<int>> coro_t;
/*
int test_coroutine_practical() {
    std::vector<int> v(10);
    coro_t::call_type coro(
        [&](coro_t::yield_type &yield) {
            for (;;) {
                v[i] += 1;
                yield();
            }
        }
    );

    int counter = 0;
    while (coro) {
        coro(v);
        std::cout << "current status of v: ";
        for (int i = 0; i < v.size(); ++i) {
            std::cout << v[i] << " ";
        }
        std::cout << std::endl;
        ++counter;
    }
    std::cout << "current status of v: ";
    for (int i = 0; i < v.size(); ++i) {
        std::cout << v[i] << " ";
    }
    std::cout << std::endl;
    return 0;
}
*/

void my_accumulate(cont_vector<double> &test, cont_vector<double> &next,
                   size_t index)
{
    splt_vector<double> test_splinter = test.detach(next);
    for (int i = 0; i < 10; ++i) {
        test_splinter.mut_comp(index, i);
    }
    test.reattach(test_splinter, next, index, index+1);
    //cont_vector<double> next = test.pull();
    //cout << "next[1] is: " << next.at(1) << endl;
}
int test_cvec()
{
    // create a cont_vector with integers 0, 1, ..., nthreads
    cont_vector<double> test(new Plus<double>());
    unsigned nthreads = thread::hardware_concurrency();
    for (unsigned i = 0; i < nthreads; ++i) {
        test.unprotected_push_back(i);
    }

    // create a copy of test; next has a unique ID, as do all cont_vectors
    auto next = test;
    // this tells test that next depends on it, for resolution purposes
    test.register_dependent(&next);
    // TODO: probably put this inside register_dependent
    test.reset_latch(std::thread::hardware_concurrency());

    // accumulate values on index comp_locus
    size_t comp_locus = 1;
    assert(comp_locus < sizeof(test));
    vector<thread> threads;
    for (unsigned i = 0; i < nthreads; ++i) {
        threads.push_back(
                thread(my_accumulate,
                       std::ref(test), std::ref(next), comp_locus)
        );
    }
    // detach threads; so, the threads run asynchronously and their scheduling
    // is determined by forces unknown (OS? std::thread?)
    for (unsigned i = 0; i < nthreads; ++i) {
        threads[i].detach();
    }
    test.resolve(next);

    // create a new cont_vector identical to next, which may not be finalized
    auto next2 = next;
    next.register_dependent(&next2);
    next.reset_latch(std::thread::hardware_concurrency());

    // accumulate again
    vector<thread> threads2;
    for (unsigned i = 0; i < nthreads; ++i) {
        threads2.push_back(
                thread(my_accumulate,
                       std::ref(next), std::ref(next2), comp_locus)
        );
    }
    for (unsigned i = 0; i < nthreads; ++i) {
        threads2[i].detach();
    }

    /* // if we sleep before checking next, likely it will have finished,
       // but not for sure
    {
        using namespace literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }
    cout << next << endl;
    */

    // we must resolve test before checking next's values
    if (next[1] != 181) {
        cerr << "! test_cvec failed:  " << next[1]
             << ", expected " << 181 << endl;
        return 1;
    }
    // we must resolve test and next before checking next2's values
    next.resolve(next2);
    if (next2[1] != 361) {
        cerr << "! test_cvec failed: at index 1, got " << next2[1]
             << ", expected " << 361 << endl;
        return 1;
    }

    cerr << "+ test_cvec passed" << endl;
    return 0;
}


void vec_timing()
{
    int test_sz = 4096;

    chrono::time_point<chrono::system_clock> st_start, st_end;
    st_start = chrono::system_clock::now();
    std::vector<double> st_test;
    for (int i = 0; i < test_sz; ++i) {
        st_test.push_back(i);
    }
    st_end = chrono::system_clock::now();

    chrono::time_point<chrono::system_clock> bp_start, bp_end;
    bp_start = chrono::system_clock::now();
    bp_vector<double> bp_test;
    for (int i = 0; i < test_sz; ++i) {
        //bp_test = bp_test.push_back(i);
        bp_test.mut_push_back(i);
    }
    bp_end = chrono::system_clock::now();

    chrono::time_point<chrono::system_clock> ps_start, ps_end;
    ps_start = chrono::system_clock::now();
    ps_vector<double> ps_test;
    for (int i = 0; i < test_sz; ++i) {
        ps_test = ps_test.push_back(i);
    }
    ps_end = chrono::system_clock::now();

    chrono::time_point<chrono::system_clock> tr_start, tr_end;
    tr_start = chrono::system_clock::now();
    tr_vector<double> tr_test;
    for (int i = 0; i < test_sz; ++i) {
        //tr_test = tr_test.push_back(i);
        tr_test.mut_push_back(i);
    }
    tr_end = chrono::system_clock::now();

    chrono::duration<double> st_dur = st_end - st_start;
    chrono::duration<double> bp_dur = bp_end - bp_start;
    chrono::duration<double> ps_dur = ps_end - ps_start;
    chrono::duration<double> tr_dur = tr_end - tr_start;

    cout << "st took " << st_dur.count()/test_sz * 1000000000 << " ns" << endl;
    cout << "bp took " << bp_dur.count()/test_sz * 1000000000 << " ns" << endl;
    cout << "ps took " << ps_dur.count()/test_sz * 1000000000 << " ns" << endl;
    cout << "tr took " << tr_dur.count()/test_sz * 1000000000 << " ns" << endl;

    /*
    int ri{42};
    atomic_int ai{42};
    cout << "sizeof(ri) -> " << sizeof(ri) << endl;
    cout << "sizeof(ai) -> " << sizeof(ai) << endl;
    */
}


/* runner *********************************************************************/

int bp_vector_runner()
{
    int ret = 0;
    std::cout << "br_sz: " << (int)br_sz << std::endl;

    vector<function<int()>> runner;
    runner.push_back(test_simple);
    runner.push_back(test_insert);
    runner.push_back(test_pers);
    runner.push_back(test_pers_alternate);
    runner.push_back(test_pers_iter);
    runner.push_back(test_trans);
    runner.push_back(test_make);
    runner.push_back(test_coroutine);
    //runner.push_back(test_coroutine_practical);
    runner.push_back(test_cvec);

    int num_tests = runner.size();
    for (int i = 0; i < num_tests; ++i) {
        ret += runner[i]();
    }
    cout << num_tests-ret << "/" << num_tests;
    if (ret == 0) {
        cout << " all tests passed!" << endl;
    }
    else {
        cout << " " << ret << " tests failed!" << endl;
    }
    vec_timing();

    return ret;
}

