#pragma once

#include "../commtypes.h"
#include "../binstream/binstream.h"

namespace coid {
//Forward declarations
template<class T> class ref_unique;
template<class T> binstream& operator << (binstream&, const ref_unique<T>&);
template<class T> binstream& operator >> (binstream&, ref_unique<T>&);

///Template making pointers uniquely owned, and to be automatically destructed when the owner is destructed
template <class Type>
class ref_unique
{
public: // methods only
    ref_unique() {}
    ~ref_unique() { if (_object_ptr) { delete _object_ptr; } }

    ref_unique(Type* object_ptr) { _object_ptr = object_ptr; }

    ref_unique(const ref_unique& object_ptr) = delete;

    //ref_unique(ref_unique&& p)
    //{
    //    _p = p._p;
    //    p._p = nullptr;
    //}

    template <class U>
    ref_unique(ref_unique<U>&& rhs)
    {
        _object_ptr = rhs.eject();
    }

    template <typename... Args>
    static ref_unique<Type> emplace(Args&&... args) {
        return ref_unique<Type>(new Type(static_cast<Args&&>(args)...));
    }

    Type* get_ptr() const { return _object_ptr; }
    Type*& get_ptr_ref() { return _object_ptr; }

    explicit operator bool() const { return _object_ptr != 0; }

    //operator T* (void) const { return _object_ptr; }

    int operator ==(const Type* ptr) const { return ptr == _object_ptr; }
    int operator !=(const Type* ptr) const { return ptr != _object_ptr; }

    Type& operator *(void) { DASSERT(_object_ptr != nullptr); return *_object_ptr; }
    const Type& operator *(void) const { DASSERT(_object_ptr != nullptr); return *_object_ptr; }

    Type* operator ->(void) { DASSERT(_object_ptr != nullptr); return _object_ptr; }
    const Type* operator ->(void) const { DASSERT(_object_ptr != nullptr); return _object_ptr; }


    ref_unique& operator = (Type* object_ptr) {
        if (_object_ptr) delete _object_ptr;
        _object_ptr = object_ptr;
        return *this;
    }

    ref_unique& operator = (ref_unique&& object_ptr)
    {
        if (_object_ptr) {
            delete _object_ptr;
        }
        swap(object_ptr);
        return *this;
    }

    friend binstream& operator << <Type>(binstream& out, const ref_unique<Type>& ptr);
    friend binstream& operator >> <Type>(binstream& in, ref_unique<Type>& ptr);

    bool is_set() const { return _object_ptr != nullptr; }
    bool is_empty() const { return _object_ptr == nullptr; }

    Type* eject() { Type* tmp = _object_ptr; _object_ptr = nullptr; return tmp; }

    ref_unique&& move() { return static_cast<ref_unique&&>(*this); }

    void destroy()
    {
        if (_object_ptr)
            delete _object_ptr;
        _object_ptr = nullptr;
    }

    void swap(ref_unique<Type>& other)
    {
        std::swap(_object_ptr, other._object_ptr);
    }

    friend void swap(ref_unique& a, ref_unique& b) {
        std::swap(a._object_ptr, b._object_ptr);
    }

protected:
    //ref_policy_base* _policy_ptr = nullptr;
    Type* _object_ptr = nullptr;
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template <class T>
binstream& operator << (binstream& out, const ref_unique<T>& ptr) {
    if (ptr.is_set())
        out << (uint)1;
    else
        out << (uint)0x00;
    if (ptr._object_ptr)  out << *ptr._object_ptr;
    return out;
}

template <class T>
binstream& operator >> (binstream& in, ref_unique<T>& ptr) {
    uint is;
    in >> is;
    if (is) {
        ptr._object_ptr = new T;
        in >> *ptr._object_ptr;
    }
    return in;
}

}; // end of namespace coid