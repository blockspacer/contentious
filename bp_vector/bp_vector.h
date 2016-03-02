#ifndef BP_VECTOR_H
#define BP_VECTOR_H

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <atomic>
#include <array>
#include <cmath>
#include <iostream>

// TODO: namespace

enum Safety { reg, persistent, transient};

constexpr uint8_t BITPART_SZ = 2;
// TODO: make these all caps
constexpr size_t br_sz = 1 << BITPART_SZ;
constexpr uint8_t br_mask = br_sz - 1;

template <typename T, template<typename> typename TDer>
class bp_vector_base;
template <typename T>
class bp_vector;
template <typename T>
class ps_vector;
template <typename T>
class tr_vector;

template <typename T>
class bp_node
{
	template <typename U, template <typename> typename TDer>
    friend class bp_vector_base;
    friend class bp_vector<T>;
	friend class ps_vector<T>;
	friend class tr_vector<T>;

private:
    //uint8_t br_i;
    std::array<std::shared_ptr<bp_node<T>>,br_sz> br;
    
    // (sz >> BITPART_SZ) + 1
    std::array<T,br_sz> val;

    int16_t id;

};


class bp_vector_glob
{
protected:
    static std::atomic<int16_t> unique_id;

};


template <typename T, template<typename> typename TDer>
class bp_vector_base : bp_vector_glob
{
    template <typename U, template <typename> typename TDerOther>
    friend class bp_vector_base;

protected:
    size_t sz;
    uint8_t shift;
    std::shared_ptr<bp_node<T>> root;
    int16_t id;
   
    // explicit default constructor
    bp_vector_base()
      : sz(0), shift(0), root(nullptr), id(0) {}
    
    // copy constructor is protected because it would allow us to create
    // transient vecs from persistent vecs without assigning a unique id
    template <template <typename> typename TDerOther>
    bp_vector_base(const bp_vector_base<T, TDerOther> &other)
      : sz(other.sz), shift(other.shift), root(other.root), id(other.id) { 
        // nothing to do here
    }
    
    // constructor that takes arbitrary id, for making transients
    bp_vector_base(int16_t id_in) 
      : sz(0), shift(0), root(nullptr), id(id_in) {
        // nothing to do here
    }
    
    inline bool node_copy(const int16_t other_id) const { 
        return static_cast<const TDer<T> *>(this)->node_copy_impl(other_id);
    };
    uint8_t calc_depth() const;

public:
    static inline int16_t get_unique_id() { return unique_id++; }

    // size-related getters
    bool empty() const;
    size_t size() const;
    uint8_t get_depth() const;
    size_t capacity() const;
    int16_t get_id() const;

    // value-related getters
    const T &operator[](size_t i) const;
    const T &at(size_t i) const;
    T &operator[](size_t i);
    T &at(size_t i);
    
    virtual TDer<T> set(const size_t i, const T &val);
    virtual TDer<T> push_back(const T &val);

};


template <typename T>
class bp_vector : public bp_vector_base<T, bp_vector>
{
private:

public:
    inline bool node_copy_impl(const int16_t) const { 
        return false; 
    }
    
    void mut_set(const size_t i, const T &val);
    void mut_push_back(const T &val);
    void insert(const size_t i, const T &val);
    T remove(const size_t i);

    //bp_vector<T> set(const size_t i, const T &val);
    //bp_vector<T> push_back(const T &val);
    //bp_vector<T> pers_insert(const size_t i, const T val);

    friend std::ostream &operator<<(std::ostream &out, const bp_vector &data)
	{
        out << "bp_vector[ ";
        for (size_t i = 0; i < data.size(); ++i) {
            out << data.at(i) << " ";
        }
        out << "]/bp_vector";
        return out;
    }

};


template <typename T>
class ps_vector : public bp_vector_base<T, ps_vector>
{
private:

public:
    ps_vector() : bp_vector_base<T, ps_vector>() {}
    ps_vector(const tr_vector<T> &other);

    inline bool node_copy_impl(const int16_t) const { 
        return true; 
    }
    
    tr_vector<T> make_transient();

    //ps_vector<T> set(const size_t i, const T &val);
    //ps_vector<T> push_back(const T &val);
    //ps_vector<T> pers_insert(const size_t i, const T val);

    friend std::ostream &operator<<(std::ostream &out, const ps_vector &data)
	{
        out << "ps_vector[ ";
        for (size_t i = 0; i < data.size(); ++i) {
            out << data.at(i) << " ";
        }
        out << "]/ps_vector";
        return out;
    }

};


template <typename T>
class tr_vector : public bp_vector_base<T, tr_vector>
{
private:

public:
    tr_vector() : bp_vector_base<T, tr_vector>() {}
    tr_vector(const ps_vector<T> &other);

    inline bool node_copy_impl(const int16_t other_id) const {
        /*
        if (this->id != other_id) { 
            std::cout << "mutating transient vec...my id: " << this->id 
                      << "; other_id: " << other_id << std::endl;
        } else {
            std::cout << "owned by us so no mutate" << std::endl;
        }
        */
        return this->id != other_id; 
    }

    ps_vector<T> make_persistent();
    
    //tr_vector<T> set(const size_t i, const T &val);
    //tr_vector<T> push_back(const T &val);
    //tr_vector<T> pers_insert(const size_t i, const T val);

    friend std::ostream &operator<<(std::ostream &out, const tr_vector &data)
	{
        out << "tr_vector[ ";
        for (size_t i = 0; i < data.size(); ++i) {
            out << data.at(i) << " ";
        }
        out << "]/tr_vector";
        return out;
    }

};

template class bp_vector_base<double, bp_vector>;
template class bp_vector<double>;
template class bp_vector_base<double, ps_vector>;
template class ps_vector<double>;
template class bp_vector_base<double, tr_vector>;
template class tr_vector<double>;

#endif  // BP_VECTOR_H
