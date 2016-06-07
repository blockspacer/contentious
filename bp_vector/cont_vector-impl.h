
template <typename T>
void cont_vector<T>::freeze(cont_vector<T> &dep,
                            bool onto,
                            std::function<int(int)> imap,
                            contentious::op<T> op,
                            uint16_t ndetached)
{
    {   // locked _data (all 3 actions must happen atomically)
        std::lock_guard<std::mutex> lock(dlck);
        // if we want our output to depend on input
        if (onto) {
            std::lock_guard<std::mutex> lock2(dep.dlck);
            dep._data = _data.new_id();
        }
        // create _used, which has the old id of _data
        tracker.emplace(std::piecewise_construct,
                        std::forward_as_tuple(&dep),
                        std::forward_as_tuple(dependency_tracker(_data, imap, op))
        );
        // modifications to data cannot affect _used
        _data = _data.new_id();
        frozen[&dep] = {};
    }
    // make a latch for reattaching splinters
    dep.rlatches.emplace(std::piecewise_construct,
                         std::forward_as_tuple(tracker[&dep]._used.get_id()),
                         std::forward_as_tuple(new boost::latch(ndetached))
    );
}

template <typename T>
void cont_vector<T>::freeze(cont_vector<T> &cont, cont_vector<T> &dep,
                            std::function<int(int)> imap)
{
    const auto &op = cont.tracker[&dep].op;
    {   // locked _data (all 3 actions must happen atomically)
        std::lock_guard<std::mutex> lock(dlck);
        // create _used, which has the old id of _data
        tracker.emplace(std::piecewise_construct,
                        std::forward_as_tuple(&dep),
                        std::forward_as_tuple(dependency_tracker(_data, imap, op))
        );
        // modifications to data cannot affect _used
        _data = _data.new_id();
        cont.frozen[&dep].push_back(this);
    }
    // make a latch for reattaching splinters
    dep.rlatches.emplace(std::piecewise_construct,
                         std::forward_as_tuple(tracker[&dep]._used.get_id()),
                         std::forward_as_tuple(new boost::latch(0))
    );
}


template <typename T>
splt_vector<T> cont_vector<T>::detach(cont_vector &dep)
{
    if (tracker.count(&dep) > 0) {
        splt_vector<T> splt(tracker[&dep]._used, tracker[&dep].op);
        {   // locked dep.reattached
            std::lock_guard<std::mutex> lock(dep.rlck);
            dep.reattached.emplace(std::make_pair(splt._data.get_id(),
                                                  false));
        }
        return splt;
    } else {
        std::cout << "DETACH ERROR: NO DEP IN TRACKER: &dep is "
                  << &dep << std::endl;
        return splt_vector<T>(tracker[&dep]._used, tracker[&dep].op);
    }
}

template <typename T>
splt_vector<T> cont_vector<T>::detach(cont_vector &dep, size_t a, size_t b)
{
    if (tracker.count(&dep) > 0) {
        {   // locked this->data
            std::lock_guard<std::mutex> lock(dlck);
            tracker[&dep]._used.copy(this->_data, a, b);
        }
        splt_vector<T> splt(tracker[&dep]._used, tracker[&dep].op);
        {   // locked dep.reattached
            std::lock_guard<std::mutex> lock(dep.rlck);
            dep.reattached.emplace(std::make_pair(splt._data.get_id(),
                                                  false));
        }
        return splt;
    } else {
        std::cout << "DETACH ERROR: NO DEP IN TRACKER: &dep is "
                  << &dep << std::endl;
        return splt_vector<T>(tracker[&dep]._used, tracker[&dep].op);
    }
}

template <typename T>
void cont_vector<T>::refresh(cont_vector &dep, size_t a, size_t b)
{
    if (tracker.count(&dep) > 0) {
        {   // locked this->data
            std::lock_guard<std::mutex> lock(dlck);
            tracker[&dep]._used.copy(this->_data, a, b);
        }
    }
}

template <typename T>
void cont_vector<T>::reattach(splt_vector<T> &splt, cont_vector<T> &dep,
                              uint16_t p, size_t a, size_t b)
{
    // TODO: generalize to non-modification operations, like
    // insert/remove/push_back/pop_back

    const int32_t sid = splt._data.get_id();
    // TODO: better (lock-free) mechanism here
    bool found = false;
    {   // locked dep.reattached
        std::lock_guard<std::mutex> lock(dep.rlck);
        found = dep.reattached.count(sid) > 0;
    }

    auto &dep_tracker = tracker[&dep];
    const int32_t uid = dep_tracker._used.get_id();
    T diff;
    if (found) {
        const auto &dep_op = dep_tracker.op;
        if (contentious::is_monotonic(dep_tracker.indexmap)) {
            std::lock_guard<std::mutex> lock(dep.dlck);
            /*{
                std::lock_guard<std::mutex> lock2(contentious::plck);
                std::cout << "copying " << splt._data.get_id() << " -> " << dep._data.get_id()
                          << " at " << a << ": ";
                auto end = splt._data.cbegin() + b;
                for (auto it = splt._data.cbegin() + a; it != end; ++it) {
                    std::cout << *it << " ";
                }
                std::cout << std::endl;
            }*/
            dep._data.copy(splt._data, a, b);
        } else {
            std::lock_guard<std::mutex> lock(dep.dlck);
            for (size_t i = a; i < b; ++i) {    // locked dep
                diff = dep_op.inv(splt._data[i], dep_tracker._used[i]);
                if (diff != dep_op.identity) {
                    dep._data.mut_set(i, dep_op.f(dep[i], diff));
                    /*std::cout << "sid " << sid
                              << " joins with " << dep._data.get_id()
                              << " with diff " << diff
                              << " to produce " << dep[i]
                              << std::endl;*/
                }
            }

        }
        {   // locked dep
            std::lock_guard<std::mutex> lock(dep.rlck);
            dep.reattached[sid] = true;
        }
    } else {
        std::cout << "TROUBLE with " << sid
                  << "!!! splinters.size(): " << dep.reattached.size()
                  << std::endl;
        std::cout << "the splinters are: ";
        for (const auto &r : dep.reattached) {
            std::cout << r.first;
        }
        std::cout << std::endl;
    }
    
    contentious::closure resolution;
    if (p == 0) {
        if (contentious::is_monotonic(dep_tracker.indexmap)) {
            resolution = std::bind(&cont_vector::resolve_monotonic,
                                   std::ref(*this), std::ref(dep));
        } else {
            resolution = std::bind(&cont_vector::resolve,
                                   std::ref(*this), std::ref(dep));
        }
        contentious::tp.submitr(resolution, p);
    }

    // TODO: erase splinters?
    //splinters.erase(sid);
    if (dep.rlatches.count(uid) > 0) {
        dep.rlatches[uid]->count_down();
    } else {
        std::cout << "didn't find latch" << std::endl;
        assert(false);
    }
}

// TODO: multithreaded and sparse
template <typename T>
void cont_vector<T>::resolve(cont_vector<T> &dep)
{
    const int32_t uid = tracker[&dep]._used.get_id();
    dep.rlatches[uid]->wait();
    
    for (auto &f : frozen[&dep]) {
        f->resolve(dep);
    }

    auto &dep_tracker = this->tracker[&dep];
    const auto &dep_op = dep_tracker.op;

    T &target = *(dep._data.begin() + dep_tracker.indexmap(0));
    auto trck = dep_tracker._used.cbegin();
    auto end = this->_data.cend();
    for (auto curr = this->_data.cbegin(); curr != end; ++curr, ++trck) {
        T diff = dep_op.inv(*curr, *trck);
        if (diff != dep_op.identity) {
            target = dep_op.f(target, diff);
        }
    }
}

/* used:
 * this
 *  tracker[&dep]
 *  _data, size()
 *  frozen
 * dep
 *  rlatches[uid]
 *  _data, size()
 */
// TODO: multithreaded and sparse
template <typename T>
void cont_vector<T>::resolve_monotonic(cont_vector<T> &dep)
{
    const int32_t uid = this->tracker[&dep]._used.get_id();
    dep.rlatches[uid]->wait();
    
    if (this->frozen.count(&dep) > 0) {
        for (auto &f : this->frozen[&dep]) {
            f->resolve_monotonic(dep);
        }
    }

    const auto &dep_tracker = this->tracker[&dep];
    const auto &dep_op = dep_tracker.op;

    size_t a, b;
    int64_t amap, o, adom, aran, bdom, bran;
    for (int pi = 0; pi < hwconc; ++pi) {
        std::tie(a, b) = contentious::partition(pi, this->size());
        amap = dep_tracker.indexmap(a);
        o = a - amap;
        adom = a + o;
        aran = a;
        if (adom < 0) {
            aran += (0 - adom);
            adom += (0 - adom);
            assert(adom == (int64_t)a);
        }
        bdom = b + o;
        bran = b;
        if (bdom >= (int64_t)this->size()) {
            bran -= (bdom - (int64_t)this->size());
            bdom -= (bdom - (int64_t)this->size());
            assert(bdom == (int64_t)b);
        }
        {   // locked this
            std::lock_guard<std::mutex> lock(rlck);
            for (int64_t check = adom; check < aran; check += 1) {
                rlist.insert(check);
            }
            for (int64_t check = bran; check < bdom; check += 1) {
                rlist.insert(check);
            }
        }
    }
    /*
    {   // locked this
        std::lock_guard<std::mutex> lock(rlck);
        std::cout << "rlist for "
                  << this->_data.get_id() << " -> " << dep._data.get_id()
                  << " with o " << o << ": ";
        for (auto it = rlist.begin(); it != rlist.end(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;
    }
    */
    {   // locked dep
        std::lock_guard<std::mutex> lock(dep.dlck);

        for (auto check = rlist.cbegin(); check != rlist.cend(); ++check) {
            int64_t cmap = dep_tracker.indexmap(*check);
            if (cmap < 0 || cmap > (int64_t)dep._data.size()) {
                continue;
            }
            auto it = dep._data.begin() + cmap;
            auto curr = this->_data.cbegin() + *check;
            auto trck = dep_tracker._used.cbegin() + *check;
            T diff = dep_op.inv(*curr, *trck);
            if (diff != dep_op.identity) {
                /*
                {
                    std::lock_guard<std::mutex> lock2(contentious::plck);
                    std::cout << this->_data.get_id() << "->" << dep._data.get_id()
                              << " would have resolved " << *check 
                              << " onto " << cmap
                              << " with curr, trck " << *curr << ", " << *trck
                              << " with diff " << diff
                              << " to produce " << *it << std::endl;
                }
                */
                *it = dep_op.f(*it, diff);
                for (int off = -1; off <= 1; ++off) {
                    if (cmap + off >= 0 && cmap + off < (int64_t)dep._data.size()) {
                        for (auto &d : tracker) {
                            d.first->rlist.insert(cmap + off);
                        }
                    }
                }
            }
        }
    }

}


/* this function performs thread function f on the passed cont_vector
 * based on a partition for processor p of hwconc
 * with extra args U... as necessary */
template <typename T>
template <typename... U>
void cont_vector<T>::exec_par(void f(cont_vector<T> &, cont_vector<T> &,
                                     const uint16_t, const U &...),
                              cont_vector<T> &dep, const U &... args)
{
    for (int p = 0; p < hwconc; ++p) {
        auto task = std::bind(f, std::ref(*this), std::ref(dep),
                              p, args...);
        contentious::tp.submit(task, p);
    }
}


template <typename T>
cont_vector<T> cont_vector<T>::reduce(const contentious::op<T> op)
{
    // our reduce dep is just one value
    auto dep = cont_vector<T>();
    dep.unprotected_push_back(op.identity);

    freeze(dep, false, contentious::alltoone<0>, op);
    // no template parameters
    exec_par<>(contentious::reduce_splt<T>, dep);

    return dep;
}

template <typename T>
cont_vector<T> cont_vector<T>::foreach(const contentious::op<T> op, const T &val)
{
    auto dep = cont_vector<T>(*this);

    // template parameter is the arg to the foreach op
    freeze(dep, true, contentious::identity, op);
    exec_par<T>(contentious::foreach_splt<T>, dep, val);

    return dep;
}

// TODO: right now, this->()size must be equal to other.size()
template <typename T>
cont_vector<T> cont_vector<T>::foreach(const contentious::op<T> op,
                                       cont_vector<T> &other)
{
    auto dep = cont_vector<T>(*this);

    // gratuitous use of reference_wrapper
    freeze(dep, true, contentious::identity, op);
    other.freeze(*this, dep, contentious::identity);
    exec_par< std::reference_wrapper<cont_vector<T>> >(
            contentious::foreach_splt_cvec<T>, dep, std::ref(other));

    return dep;
}

template <typename T>
cont_vector<T> cont_vector<T>::stencil(const std::vector<int> &offs,
                                       const std::vector<T> &coeffs,
                                       const contentious::op<T> op1,
                                       const contentious::op<T> op2)
{
    /*
     * part 1
     */
    // get unique coefficients
    auto coeffs_unique = coeffs;
    auto it = std::unique(coeffs_unique.begin(), coeffs_unique.end());
    coeffs_unique.resize(std::distance(coeffs_unique.begin(), it));

    // perform coefficient multiplications on original vector
    // TODO: limit range of multiplications to only those necessary
    std::map<T, cont_vector<T>> step1;
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        step1.emplace(std::piecewise_construct,
                      std::forward_as_tuple(coeffs_unique[i]),
                      std::forward_as_tuple(*this)
        );
        freeze(step1.at(coeffs_unique[i]), true, contentious::identity, op1);
        exec_par<T>(contentious::foreach_splt<T>,
                    step1.at(coeffs_unique[i]),
                    coeffs_unique[i]);
    }
    /*
     * part 2
     */
    // sum up the different parts of the stencil
    // TODO: move inside for loop to correct semantics?
    std::vector<cont_vector<T>> step2;
    step2.reserve(offs.size()*2 + 1);
    step2.emplace_back(*this);
    for (size_t i = 0; i < offs.size(); ++i) {
        step2.emplace_back(step2[i]);
        step2[i].freeze(step2[i+1], true, contentious::identity, op2);
        if (i == 0) {
            step1.at(coeffs[i]).freeze(step2[i], step2[i+1], contentious::offset<-1>);
        } else {
            step1.at(coeffs[i]).freeze(step2[i], step2[i+1], contentious::offset<+1>);
        }
        step2[i].template exec_par<std::reference_wrapper<cont_vector<T>>>(
                contentious::foreach_splt_cvec<T>,
                step2[i+1], std::ref(step1.at(coeffs[i]))
        );
    }

    auto &thisthis = step2[step2.size()-1];
    std::map<T, cont_vector<T>> step11;
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        step11.emplace(std::piecewise_construct,
                       std::forward_as_tuple(coeffs_unique[i]),
                       std::forward_as_tuple(thisthis)
        );
        thisthis.freeze(step11.at(coeffs_unique[i]), true, contentious::identity, op1);
        thisthis.exec_par<T>(contentious::foreach_splt<T>,
                             step11.at(coeffs_unique[i]),
                             coeffs_unique[i]);
    }
    for (size_t i = offs.size(); i < offs.size()*2; ++i) {
        step2.emplace_back(step2[i]);
        step2[i].freeze(step2[i+1], true, contentious::identity, op2);
        if (i == offs.size()) {
            step11.at(coeffs[0]).freeze(step2[i], step2[i+1], contentious::offset<-1>);
        } else {
            step11.at(coeffs[1]).freeze(step2[i], step2[i+1], contentious::offset<+1>);
        }
        step2[i].template exec_par<std::reference_wrapper<cont_vector<T>>>(
                contentious::foreach_splt_cvec<T>,
                step2[i+1], std::ref(step11.at(coeffs[i-offs.size()]))
        );
    }

    contentious::tp.finish();

    //std::cout << "input: " << step2[0] << std::endl;
    // all the resolutions... *exhale*
    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        //resolve(step1.at(coeffs_unique[i]));
        //std::cout << "coeff vec: " << step1.at(coeffs_unique[i]) << std::endl;
    }
    for (size_t i = 0; i < offs.size(); ++i) {
        //step2[i].resolve(step2[i+1]);
        //std::cout << "step2: " << step2[i+1] << std::endl;
    }

    for (size_t i = 0; i < coeffs_unique.size(); ++i) {
        //thisthis.resolve(step11.at(coeffs_unique[i]));
        //std::cout << "coeff vec: " << step11.at(coeffs_unique[i]) << std::endl;
    }
    for (size_t i = offs.size(); i < offs.size()*2; ++i) {
        //step2[i].resolve(step2[i+1]);
        //std::cout << "step2: " << step2[i+1] << std::endl;
    }
    return step2[step2.size()-1];
}

