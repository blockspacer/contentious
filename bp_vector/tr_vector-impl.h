template <typename T>
void tr_vector<T>::mut_set(const size_t i, const T &val)
{
    if (this->id != this->root->id) {
        this->root = new bp_node<T>(*this->root, this->id);
    }
    bp_node<T> *node = this->root.get();
    for (uint16_t s = this->shift; s > 0; s -= BP_BITS) {
        bp_node_ptr<T> &next = node->as_branches()[i >> s & BP_MASK];
        if (this->id != next->id) {
            next = new bp_node<T>(*next, this->id);
        }
        node = node->as_branches()[i >> s & BP_MASK].get();
    }
    node->as_leaves()[i & BP_MASK] = val;
}

template <typename T>
void tr_vector<T>::mut_push_back(const T &val)
{
    if (this->sz % BP_WIDTH != 0) {
        mut_set(this->sz++, val);
        return;
    }

    if (this->sz == 0) {
        this->root = new bp_node<T>(bp_node_t::leaves, this->id);
        this->root->as_leaves()[this->sz++] = val;
        return;
    }

    size_t depth_cap = this->capacity();
    int16_t depth_ins = -1;
    while (this->sz % depth_cap != 0) {
        ++depth_ins;
        depth_cap /= BP_WIDTH;
    }

    if (this->id != this->root->id) {
        this->root = new bp_node<T>(*this->root, this->id);
    }

    if (depth_ins == -1) {
        this->shift += BP_BITS;
        bp_node_ptr<T> temp = new bp_node<T>(bp_node_t::branches, this->id);
        this->root.swap(temp);
        this->root->as_branches()[0] = std::move(temp);
    }

    bp_node<T> *node = this->root.get();
    uint16_t s = this->shift;
    while (depth_ins > 0) {
        bp_node_ptr<T> &next = node->as_branches()[this->sz >> s & BP_MASK];
        assert(next);
        if (this->id != next->id) {
            next = new bp_node<T>(*next, this->id);
        }
        node = next.get();
        s -= BP_BITS;
        --depth_ins;
    }
    assert(s <= this->shift && s >= 0);

    while (s > BP_BITS) {
        bp_node_ptr<T> &next = node->as_branches()[this->sz >> s & BP_MASK];
        next = new bp_node<T>(bp_node_t::branches, this->id);
        node = next.get();
        s -= BP_BITS;
    }
    if (s == BP_BITS) {
        bp_node_ptr<T> &next = node->as_branches()[this->sz >> s & BP_MASK];
        next = new bp_node<T>(bp_node_t::leaves, this->id);
        node = next.get();
        s -= BP_BITS;
    }

    // add value
    node->as_leaves()[this->sz & BP_MASK] = val;
    ++this->sz;
}
