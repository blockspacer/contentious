// mutating versions

template <typename T, template<typename> typename TDer>
using bp_vector_base_ptr = boost::intrusive_ptr<bp_vector_base<T, TDer>>;
template <typename T>
using bp_node_ptr = boost::intrusive_ptr<bp_node<T>>;


template <typename T>
tr_vector<T> bp_vector<T>::make_transient() const
{
    tr_vector<T> ret(*this);
    return ret;
}

template <typename T>
void bp_vector<T>::mut_set(const size_t i, const T &val)
{
    bp_node<T> *node = this->root.get();
    for (uint16_t s = this->shift; s > 0; s -= BP_BITS) {
        node = node->branches[i >> s & BP_MASK].get();
    }
    node->values[i & BP_MASK] = val;
}

template <typename T>
void bp_vector<T>::mut_push_back(const T &val)
{
    if (this->sz % BP_WIDTH != 0) {
        mut_set(this->sz++, val);
        return;
    }

    size_t depth_cap = this->capacity();
    int16_t depth_ins = -1;
    while (this->sz % depth_cap != 0) {
        ++depth_ins;
        depth_cap /= BP_WIDTH;
    }

    if (depth_ins == -1) {
        this->shift += BP_BITS;
        bp_node_ptr<T> temp = new bp_node<T>();
        this->root.swap(temp);
        this->root->branches[0] = std::move(temp);
    }

    bp_node<T> *node = this->root.get();
    uint16_t s = this->shift;
    while (depth_ins > 0) {
        bp_node_ptr<T> &next = node->branches[this->sz >> s & BP_MASK];
        if (!next) {
            std::cout << "Constructing node where one should have already been"
                      << std::endl;
            next = new bp_node<T>();
        }
        node = next.get();
        s -= BP_BITS;
        --depth_ins;
    }
    assert(s <= this->shift && s >= 0);

    while (s > BP_BITS) {
        bp_node_ptr<T> &next = node->branches[this->sz >> s & BP_MASK];
        next = new bp_node<T>();
        node = next.get();
        s -= BP_BITS;
    }
    if (s == BP_BITS) {
        bp_node_ptr<T> &next = node->branches[this->sz >> s & BP_MASK];
        next = new bp_node<T>();
        node = next.get();
        s -= BP_BITS;
    }

    // add value
    node->values[this->sz & BP_MASK] = val;
    ++this->sz;
}
