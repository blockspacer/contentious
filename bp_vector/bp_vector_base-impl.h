
#include <iostream>
#include <string>
#include <stdexcept>

#include <cmath>
#include <cassert>

#include "util.h"


template <typename T, template<typename> typename TDer>
using bp_vector_base_ptr = boost::intrusive_ptr<bp_vector_base<T, TDer>>;
template <typename T>
using bp_node_ptr = boost::intrusive_ptr<bp_node<T>>;

template <typename T>
using branches = std::array<boost::intrusive_ptr<bp_node<T>>,br_sz>;
template <typename T>
using values = std::array<T, br_sz>;


template <typename T, template<typename> typename TDer>
uint8_t bp_vector_base<T, TDer>::calc_depth() const
{
    return shift / BITPART_SZ + 1;
}

template <typename T, template<typename> typename TDer>
bool bp_vector_base<T, TDer>::empty() const
{
    return sz == 0;
}    

template <typename T, template<typename> typename TDer>
size_t bp_vector_base<T, TDer>::size() const
{
    return sz;
}    

template <typename T, template<typename> typename TDer>
uint8_t bp_vector_base<T, TDer>::get_depth() const
{
    return calc_depth();
}

template <typename T, template<typename> typename TDer>
size_t bp_vector_base<T, TDer>::capacity() const
{
    if (sz == 0) {
        return 0;
    }
    return pow(br_sz, calc_depth());
}

template <typename T, template<typename> typename TDer>
int16_t bp_vector_base<T, TDer>::get_id() const
{
    return id;
}

template <typename T, template<typename> typename TDer>
const T &bp_vector_base<T, TDer>::operator[](size_t i) const
{
    const bp_node<T> *node = root.get();
    for (int16_t s = shift; s > 0; s -= BITPART_SZ) {
        node = boost::get<branches<T>>(node->br)[i >> s & br_mask].get();
    }
    return boost::get<values<T>>(node->br)[i & br_mask];
}

template <typename T, template<typename> typename TDer>
const T &bp_vector_base<T, TDer>::at(size_t i) const
{
    if (i >= sz) {  // presumably, throw an exception... 
        throw std::out_of_range("trie has size " + std::to_string(sz) +
                                ", given index " + std::to_string(i));
    }
    return this->operator[](i);
}

template <typename T, template<typename> typename TDer>
T &bp_vector_base<T, TDer>::operator[](size_t i)
{
    // boilerplate implementation in terms of const version
    return const_cast<T &>(
      implicit_cast<const bp_vector_base<T, TDer> *>(this)->operator[](i));
}

template <typename T, template<typename> typename TDer>
T &bp_vector_base<T, TDer>::at(size_t i)
{
    // boilerplate implementation in terms of const version
    return const_cast<T &>(
      implicit_cast<const bp_vector_base<T, TDer> *>(this)->at(i));
}

// undefined behavior if i >= sz
template <typename T, template<typename> typename TDer>
TDer<T> bp_vector_base<T, TDer>::set(const size_t i, const T &val)
{
    // copy trie
    //TDer<T> ret = *this;
    TDer<T> ret;
    ret.sz = this->sz;
    ret.shift = this->shift;
    ret.root = this->root;
    ret.id = this->id;
    // copy root node and get it in a variable
    if (node_copy(ret.root->id)) {
        ret.root = new bp_node<T>(*root);
        ret.root->id = id;
    }
    bp_node<T> *node = ret.root.get();
    for (int16_t s = shift; s > 0; s -= BITPART_SZ) {
        bp_node_ptr<T> &next = 
            boost::get<branches<T>>(node->br)[i >> s & br_mask];
        if (node_copy(next->id)) {
            next = new bp_node<T>(*next);
            next->id = id;
        }
        node = next.get();
    }
    boost::get<values<T>>(node->br)[i & br_mask] = val;
    return ret;
}

template <typename T, template<typename> typename TDer>
TDer<T> bp_vector_base<T, TDer>::push_back(const T &val)
{
    // just a set; only 1/br_sz times do we even have to construct nodes
    if (this->sz % br_sz != 0) {
        TDer<T> ret = this->set(this->sz, val);
        ++(ret.sz);
        return ret;
    }
    
    //TDer<T> ret = *this;
    TDer<T> ret;
    ret.sz = this->sz;
    ret.shift = this->shift;
    ret.root = this->root;
    ret.id = this->id;
    
    // simple case for empty trie
    if (sz == 0) {
        ret.root = new bp_node<T>(values<T>());
        ret.root->id = id;
        boost::get<values<T>>(ret.root->br)[ret.sz++] = val;
        //std::cout << "root node size: " << sizeof(*(ret.root)) << std::endl;
        return ret;
    }
    
    // we're gonna have to construct new nodes, or rotate
    uint8_t depth = calc_depth();
    // subvector capacity at this depth
    size_t depth_cap = pow(br_sz, depth);
    // depth at which to insert new node
    int16_t depth_ins = -1;
    // figure out how deep we must travel to branch
    while (sz % depth_cap != 0) {
        ++depth_ins;
        depth_cap /= br_sz;
    }
    
    if (node_copy(ret.root->id)) {
        ret.root = new bp_node<T>(*root);
        ret.root->id = id;
    }

    // must rotate trie, as it's totally full (new root, depth_ins is -1)
    if (depth_ins == -1) {
        // update appropriate values
        ret.shift += BITPART_SZ;
        ++depth;
        // rotate trie
        boost::intrusive_ptr<bp_node<T>> temp = new bp_node<T>(branches<T>());
        temp->id = id;
        ret.root.swap(temp);
        boost::get<branches<T>>(ret.root->br)[0] = std::move(temp);
    } 

    // travel to branch of trie where new node will be constructed (if any)
    bp_node<T> *node = ret.root.get();
    int16_t s = ret.shift;
    while (depth_ins > 0) {
        bp_node_ptr<T> &next = 
            boost::get<branches<T>>(node->br)[sz >> s & br_mask];
        if (!next) {
            std::cout << "Constructing node where one should have already been" 
                      << std::endl;
            next = new bp_node<T>(branches<T>());
            next->id = id;
        }
        if (node_copy(next->id)) {
            next = new bp_node<T>(*next);
            next->id = id;
        }
        node = next.get();
        s -= BITPART_SZ;
        --depth_ins;
    }
    // we're either at the top, the bottom or somewhere in-between...
    assert(s <= ret.shift && s >= 0);
    
    // keep going, but this time, construct new nodes as necessary
    while (s > BITPART_SZ) {
        bp_node_ptr<T> &next = 
            boost::get<branches<T>>(node->br)[sz >> s & br_mask];
        next = new bp_node<T>(branches<T>());
        next->id = id;
        node = next.get();
        s -= BITPART_SZ;
    }
    if (s > 0) {
        bp_node_ptr<T> &next = 
            boost::get<branches<T>>(node->br)[sz >> s & br_mask];
        next = new bp_node<T>(values<T>());
        next->id = id;
        node = next.get();
        s -= BITPART_SZ;
    }

    // add value
    boost::get<values<T>>(node->br)[sz & br_mask] = val;
    ++ret.sz;
    return ret;
}

