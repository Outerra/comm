#ifndef __COID_COMM_UNIQUE__HEADER_FILE__
#define __COID_COMM_UNIQUE__HEADER_FILE__

#include "namespace.h"

#include "commtypes.h"
#include "binstream/binstream.h"
//#include <memory>


COID_NAMESPACE_BEGIN

//Forward declarations
template<class T> class unique_ptr;
template<class T> binstream& operator << (binstream&, const unique_ptr<T>&);
template<class T> binstream& operator >> (binstream&, unique_ptr<T>&);

///Template making pointers uniquely owned, and to be automatically destructed when the owner is destructed
template <class T>
class unique_ptr
{
    T* _p = nullptr;
public:
    unique_ptr() {}
    ~unique_ptr() { if (_p) { delete _p; } }

    unique_ptr(T* p) { _p = p; }

    unique_ptr(const unique_ptr& p) = delete;

    //unique_ptr(unique_ptr&& p)
    //{
    //    _p = p._p;
    //    p._p = nullptr;
    //}

    template <class U>
    unique_ptr(unique_ptr<U>&& p)
    {
        _p = p.eject();
    }

    template <typename... Args>
    static unique_ptr<T> emplace(Args&&... args) {
        return unique_ptr<T>(new T(static_cast<Args&&>(args)...));
    }

    T* get_ptr() const { return _p; }
    T*& get_ptr_ref() { return _p; }

    explicit operator bool() const { return _p != 0; }

    //operator T* (void) const { return _p; }

    int operator ==(const T* ptr) const { return ptr == _p; }
    int operator !=(const T* ptr) const { return ptr != _p; }

    T& operator *(void) {DASSERT(_p != nullptr); return *_p; }
    const T& operator *(void) const { DASSERT(_p != nullptr); return *_p; }
  
    T* operator ->(void) { DASSERT(_p != nullptr); return _p; }
    const T* operator ->(void) const { DASSERT(_p != nullptr); return _p; }


    unique_ptr& operator = (T* p) {
        if (_p) delete _p;
        _p = p;
        return *this;
    }

    unique_ptr& operator = (unique_ptr&& p)
    {
        if (_p) {
            delete _p;
        }
        swap(p);
        return *this;
    }

    friend binstream& operator << <T>(binstream& out, const unique_ptr<T>& ptr);
    friend binstream& operator >> <T>(binstream& in, unique_ptr<T>& ptr);

    bool is_set() const { return _p != nullptr; }
    bool is_empty() const { return _p == nullptr; }

    T* eject() { T* tmp = _p; _p = nullptr; return tmp; }

    unique_ptr&& move() { return static_cast<unique_ptr&&>(*this); }

    void destroy()
    {
        if (_p)
            delete _p;
        _p = nullptr;
    }

    void swap(unique_ptr<T>& other)
    {
        std::swap(_p, other._p);
    }

    friend void swap(unique_ptr& a, unique_ptr& b) {
        std::swap(a._p, b._p);
    }
};

template <class T>
binstream& operator << (binstream& out, const unique_ptr<T>& ptr) {
    if (ptr.is_set())
        out << (uint)1;
    else
        out << (uint)0x00;
    if (ptr._p)  out << *ptr._p;
    return out;
}

template <class T>
binstream& operator >> (binstream& in, unique_ptr<T>& ptr) {
    uint is;
    in >> is;
    if (is) {
        ptr._p = new T;
        in >> *ptr._p;
    }
    return in;
}

COID_NAMESPACE_END

#endif // __COID_COMM_UNIQUE__HEADER_FILE__
