
#include "test_constants.h"
#include "cont_vector-tests.h"
#include "slbench.h"
#include "contentious/cont_vector.h"

#include <iostream>
#include <fstream>
#include <random>
#include <algorithm>

using namespace std;

/* old, but useful for testing ************************************************/

void cont_inc_reduce(cont_vector<double> &cont_ret, uint16_t p,
                     vector<double>::const_iterator a,
                     vector<double>::const_iterator b)
{
    splt_vector<double> splt_ret = cont_ret.detach(cont_ret, p);
    double temp(0);
    for (auto it = a; it != b; ++it) {
        temp += *it;
    }
    splt_ret.mut_comp(0, temp);

    /*
    double avx_ret(0);
    __m256d vals = _mm256_set_pd(0, 0, 0, 0);
    __m256d temps;
    for (int64_t i = 0; i < n; i += 4) {
        temps = _mm256_set_pd(1, 1, 1, 1);
        vals = _mm256_add_pd(vals, temps);
    }
    __m256d hsum = _mm256_hadd_pd(vals, vals);
    __m256d perm = _mm256_permute2f128_pd(hsum, hsum, 0x21);
    __m256d mret = _mm256_add_pd(hsum, perm);
    __m128d temp = _mm256_extractf128_pd(mret, 0);
    _mm_storel_pd(&avx_ret, temp);
    splt_ret.comp(0, avx_ret);
    */

    cont_vector<double> next = cont_vector<double>(cont_ret);
    cont_ret.freeze(next, contentious::identity, contentious::plus<double>);
    cont_ret.reattach(splt_ret, next, p, 0, next.size());
}
double cont_reduce_manual(const vector<double> &test_vec)
{
    cont_vector<double> cont_ret;
    cont_ret.unprotected_push_back(0);
    vector<thread> cont_threads;
    uint16_t nthreads = contentious::HWCONC;
    size_t chunk_sz = test_vec.size()/nthreads;
    for (uint16_t p = 0; p < nthreads; ++p) {
        cont_threads.push_back(
          thread(cont_inc_reduce,
                 std::ref(cont_ret), p,
                 test_vec.begin() + chunk_sz * (p),
                 test_vec.begin() + chunk_sz * (p+1)));
    }
    for (uint16_t p = 0; p < nthreads; ++p) {
        cont_threads[p].join();
    }

    return cont_ret[0];
}

/* new, uses members of cont_vector *******************************************/

void cont_reduce(const vector<double> &test_vec)
{
    cont_vector<double> cont_inp;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        cont_inp.unprotected_push_back(test_vec[i]);
    }
    auto cont_ret = cont_inp.reduce(contentious::plus<double>);
    contentious::tp.finish();
    cout << cont_ret[0] << endl;
}

vector<double> stdv_foreach(const vector<double> &test_vec,
                            const vector<double> &other_vec)
{
    auto ret = test_vec;
    for_each(ret.begin(), ret.end(),
             [](double &d){ d *= 2; }
    );
    auto ret2 = ret;
    auto oit = other_vec.cbegin();
    for_each(ret2.begin(), ret2.end(),
             [&oit](double &d){ d *= *oit; ++oit; }
    );
    return ret2;
}

shared_ptr<cont_vector<double>> cont_foreach(cont_vector<double> &test_cvec,
                                             cont_vector<double> &other_cvec)
{
    auto ret1 = test_cvec.foreach(contentious::mult<double>, 2);
    auto ret2 = ret1->foreach(contentious::mult<double>, other_cvec);
    contentious::tp.finish();
    return ret2;
}

int cont_stencil(const vector<double> &)
{
    cont_vector<double> cont_inp;

    cont_inp.unprotected_push_back(/*test_vec[i]*/0);
    for (size_t i = 1; i < 63/*(test_vec.size()*/; ++i) {
        cont_inp.unprotected_push_back(/*test_vec[i]*/1);
    }
    cont_inp.unprotected_push_back(/*test_vec[i]*/0);
    //cout << "cont_inp: " << cont_inp << endl;
    auto cont_ret = cont_inp.stencil<-1, 1>({0.5, 0.5});
    auto cont_ret2 = cont_ret->stencil<-1, 1>({0.5, 0.5});
    auto cont_ret3 = cont_ret2->stencil<-1, 1>({0.5, 0.5});
    auto cont_ret4 = cont_ret3->stencil<-1, 1>({0.5, 0.5});
    auto cont_ret5 = cont_ret4->stencil<-1, 1>({0.5, 0.5});
    auto cont_ret6 = cont_ret5->stencil<-1, 1>({0.5, 0.5});
    auto cont_ret7 = cont_ret6->stencil<-1, 1>({0.5, 0.5});
    auto cont_ret8 = cont_ret7->stencil<-1, 1>({0.5, 0.5});
    auto cont_ret9 = cont_ret8->stencil<-1, 1>({0.5, 0.5});
    //cout << cont_ret4 << endl;
    contentious::tp.finish();

    //cout << *cont_ret << endl;
    //cout << *cont_ret2 << endl;
    cout << *cont_ret3 << endl;
    cout << *cont_ret9 << endl;
    /*int bad = 0;
    for (size_t i = 0; i < cont_ret.size(); ++i) {
        double change = 1;
        if (i > 0) { change += cont_inp[i-1]*0.5; }
        if (i < cont_ret.size()-1) { change += cont_inp[i+1]*0.5; }
        // o the humanity
        if (abs(cont_inp[i] + change - (cont_ret)[i]) > 0.00000001) {
            ++bad;
            cout << "bad resolution at cont_ret[" << i << "]: "
                 << cont_inp[i] * change << " "
                 << cont_ret[i] << endl;
            return 1;
        }
    }
    if (bad != 0) {
        cout << "bad resolutions for stencil: " << bad << endl;
    }*/
    //cout << "cont_ret: " << cont_ret << endl;
    return 0;
}

constexpr double ipow(double base, int exp, double result = 1)
{
    return exp < 1 ? result : \
               ipow(base*base, exp/2, (exp % 2) ? result*base : result);
}

vector<double> stdv_heat(vector<double> &inp,
                         const int64_t r, const int64_t c, const double s)
{
    /*using namespace std::placeholders;
    function<double (double,double)> shift =
               std::bind(multplus_fp<double>, _1, _2, s);
    function<double (double,double)> center =
               std::bind(multplus_fp<double>, _1, _2, -2*s);*/
    array<unique_ptr<vector<double>>, 9> grid;
    grid[0] = make_unique<vector<double>>(inp);
    for (int i = 1; i < r; ++i) {
        int icurr = i % 9;
        int iprev = (i-1) % 9;
        grid[icurr] = make_unique<vector<double>>(*grid[iprev]);
        //(*grid[n])[0] += s*(0 - 2*(*grid[n-1])[0] + (*grid[n-1])[1]);
        for (int j = 1; j < c-1; ++j) {
            /*grid[n][j] = shift(grid[n][j], grid[n-1][j-1]);
            grid[n][j] = center(grid[n][j], grid[n-1][j]);
            grid[n][j] = shift(grid[n][j], grid[n-1][j+1]);*/
            (*grid[icurr])[j] += s*((*grid[iprev])[j-1] -
                                     2*(*grid[iprev])[j] +
                                     (*grid[iprev])[j+1]);
        }
        //(*grid[n])[c-1] += s*((*grid[n-1])[c-2] - 2*(*grid[n-1])[c-1]);
    }
    return *grid[(r-1) % 9];
}

shared_ptr<cont_vector<double>> cont_heat(cont_vector<double> &cont_inp,
                                          const int64_t r, const double s)
{
#ifdef CTTS_STATS
    for (uint16_t p = 0; p < contentious::HWCONC; ++p) {
        contentious::splt_durs[p].vals.clear();
        contentious::rslv_durs[p].vals.clear();
        contentious::conflicted[p].emplace_back(0);
        contentious::rslv_series[p].start(chrono::steady_clock::now());
    }
#endif
    constexpr int t_store = 513;
    array<shared_ptr<cont_vector<double>>, t_store> grid;
    grid[0] = make_shared<cont_vector<double>>(cont_inp);
    for (int t = 1; t < r; ++t) {
        int icurr = t % t_store;
        int iprev = (t-1) % t_store;
        grid[icurr] = grid[iprev]->stencil<-1, 0, 1>({1.0*s, -2.0*s, 1.0*s});
        if (t % (t_store-1) == (t_store-2)) {
            contentious::tp.finish();
        }
    }
    contentious::tp.finish();
#ifdef CTTS_STATS
    for (uint16_t p = 0; p < contentious::HWCONC; ++p) {
        using namespace fmt::literals;
        contentious::rslv_series[p].log("seriesdata_{}.log"_format(p));
        contentious::rslv_series[p].vals.clear();
        contentious::rslv_series[p].data.clear();
    }
#endif
    return grid[(r-1) % t_store];
}


/* runner *********************************************************************/

int cont_vector_runner()
{
	constexpr int16_t f = cont_testing::mini;
    constexpr int64_t test_sz = ipow(2,15+f) * 3;
    static_assert(test_sz > 0, "Must run with test size > 0");
    //const int16_t test_n = ipow(2,14-f);

    //cout << "**** Testing cont_vector with size: " << test_sz << endl;

    random_device rnd_device;
    mt19937 mersenne_engine(rnd_device());
    uniform_real_distribution<double> dist(-32.768, 32.768);
    // std::vector with random doubles
    auto gen = std::bind(dist, mersenne_engine);
    vector<double> test_vec(test_sz);
    generate(begin(test_vec), end(test_vec), gen);
    cont_vector<double> test_cvec;
    // cont_vector with random doubles
    for (size_t i = 0; i < test_vec.size(); ++i) {
        test_cvec.unprotected_push_back(test_vec[i]);
    }
    // helper vectors for foreach tests
    vector<double> other_vec;
    for (size_t i = 0; i < test_vec.size(); ++i) {
        other_vec.push_back(i+1);
    }
    cont_vector<double> other_cvec;
    for (size_t i = 0; i < test_cvec.size(); ++i) {
        other_cvec.unprotected_push_back(i+1);
    }
    
    // parameters for heat equation tests
    constexpr double dy = 0.05;
    constexpr double dt = 0.0005;
    constexpr double y_max = 400000/4/4/*/4/4/4/4*/;
    constexpr double t_max = 0.5;
    constexpr double viscosity = 2.0 * 1.0/ipow(10,0);
    constexpr int64_t c = (y_max + dy) / dy;
    constexpr int64_t r = (t_max + dt) / dt;
    //constexpr double V0 = 10;
    constexpr double s = viscosity * dt/ipow(dy,2);

    // vectors for heat equation tests
    vector<double> heat_vec;
    cont_vector<double> heat_cvec;
    heat_vec.push_back(sin(0) + 2.7);
    heat_cvec.unprotected_push_back(sin(0) + 2.7);
    for (int64_t i = 1; i < c; ++i) {
        double val = sin(dy*i) + 2.7;
        heat_vec.push_back(val);
        heat_cvec.unprotected_push_back(val);
    }

    /*for (int i = 0; i < 1; ++i) {
        cont_stencil(test_vec);
    }*/
    cout << "**** Testing heat equation with (c,r,s): " << c << "," << r << "," << s << endl;

    slbench::suite<vector<double>> stdv_suite {
        /*{"stdv_foreach", slbench::make_bench<test_n>(stdv_foreach,
                                                     test_vec, other_vec)   }
       ,*/{"stdv_heat",    slbench::make_bench<12>(stdv_heat, heat_vec, r, c, s) }
    };
    auto stdv_output = slbench::run_suite(stdv_suite);

    slbench::suite<shared_ptr<cont_vector<double>>> cont_suite {
        /*{"cont_foreach", slbench::make_bench<test_n>(cont_foreach,
                                                       test_cvec, other_cvec) }
       ,*/{"cont_heat",    slbench::make_bench<12>(cont_heat, heat_cvec, r, s) }
    };
    auto cont_output = slbench::run_suite(cont_suite);

    cout << stdv_output << endl;
    cout << cont_output << endl;

    {
        using namespace fmt::literals;
        slbench::log_output("heat_{}_{}_{}.log"_format(
                            contentious::HWCONC, c, r),
                            stdv_output);
        slbench::log_output("heat_{}_{}_{}.log"_format(
                            contentious::HWCONC, c, r),
                            cont_output);
    }

    /*size_t foreach_bad = 0;
    double tol = 0.000001;
    auto stdv_ans = stdv_output["stdv_foreach"].res;
    auto cont_ans = cont_output["cont_foreach"].res;
    for (size_t i = 0; i < stdv_ans.size(); ++i) {
        if (abs(stdv_ans[i] - (*cont_ans)[i]) > tol) {
            ++foreach_bad;
        }
    }
    if (foreach_bad > 0) {
        cout << "Bad vals of foreach: " << foreach_bad << endl;
    } else {
        cout << "No bad vals of foreach" << endl;
    }*/

    size_t heat_bad = 0;
    double tol = 0.000001;
    auto stdv_ans = stdv_output["stdv_heat"].res;
    auto cont_ans = cont_output["cont_heat"].res;
    for (size_t i = 0; i < stdv_ans.size(); ++i) {
        if (abs(stdv_ans[i] - (*cont_ans)[i]) > tol) {
            std::cout << "bad val of heat at: " << i << ", " << stdv_ans[i] << ", " << (*cont_ans)[i] << std::endl;
            ++heat_bad;
        }
    }
    if (heat_bad > 0) {
        cout << "Bad vals of heat: " << heat_bad << endl;
    } else {
        cout << "No bad vals of heat" << endl;
    }

    size_t Y = 10;
    size_t y_skip = cont_ans->size()/Y;
    for (auto it = heat_cvec.cbegin(); it != heat_cvec.cend(); it += y_skip) {
        cout << *it << " ";
    }
    cout << endl;
    for (auto it = cont_ans->cbegin(); it != cont_ans->cend(); it += y_skip) {
        cout << *it << " ";
    }
    cout << endl;
    for (auto it = stdv_ans.cbegin(); it < stdv_ans.cend(); it += y_skip) {
        cout << *it << " ";
    }
    cout << endl;

    return 0;
}

