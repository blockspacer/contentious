#ifndef PS_VEC_TRIE
#define PS_VEC_TRIE

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <array>
#include <cmath>

// br means branches
    

constexpr uint8_t BITPART_SZ = 5;
constexpr size_t br_sz = 1 << BITPART_SZ;
constexpr uint8_t br_mask = br_sz - 1;

template <typename T>
class PS_Trie;

template <typename T>
class PS_Node
{
    friend class PS_Trie<T>;

private:
    
    //uint8_t br_i;
    std::array<std::shared_ptr<PS_Node<T>>,br_sz> br;
    
    // (sz >> BITPART_SZ) + 1
    std::array<T,br_sz> val;
};
 

template <typename T>
class PS_Trie
{

private:

    size_t sz = 0;
    uint8_t shift = 0;
    std::shared_ptr<PS_Node<T>> root;
    
    uint8_t calc_depth() const;

public:
    
    // size-related getters
    bool empty() const;
    size_t size() const;
    uint8_t get_depth() const;
    size_t capacity() const;

    // value-related getters
    T get(const size_t i) const;
    
    void set(const size_t i, const T val);
    void push_back(const T val);
    void insert(const size_t i, const T val);
    T remove(const size_t i);

    PS_Trie<T> pers_set(const size_t i, const T val);
    PS_Trie<T> pers_push_back(const T val);
    //PS_Trie<T> pers_insert(const size_t i, const T val);
};


template class PS_Trie<double>;

#endif // PS_VEC_TRIE

