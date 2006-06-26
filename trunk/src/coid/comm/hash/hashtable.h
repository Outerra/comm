/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is COID/comm module.
 *
 * The Initial Developer of the Original Code is
 * PosAm.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brano Kemen
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef __COID_COMM_HASHTABLE__HEADER_FILE__
#define __COID_COMM_HASHTABLE__HEADER_FILE__


#include "../namespace.h"
#include "../dynarray.h"
#include "../metastream/metastream.h"
#include <algorithm>

#include "hashfunc.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
template <class T>
struct _Select_Itself{
    typedef const T&    retType;

    retType operator()(const T& t) const { return t; }
};

template <class PAIR, class KEY>
struct _Select_pair1st
{
    typedef const KEY&  retType;

    retType operator() (const PAIR& __x) const  { return __x.first; }
};

////////////////////////////////////////////////////////////////////////////////
template<class KEYSTORE, class KEYLOOKUP=KEYSTORE>
struct equal_to	: public std::binary_function<KEYSTORE, KEYLOOKUP, bool>
{
	bool operator()(const KEYSTORE& _Left, const KEYLOOKUP& _Right) const
    {
        return (_Left == _Right);
    }
};

////////////////////////////////////////////////////////////////////////////////
template <class VAL, class KEY, class HASHFUNC, class EQFUNC, class GETKEYFUNC, class ALLOC>
class hashtable
{
public:
    ///Type used for lookups. Usually the same as LOOKUP_KEY, but sometimes can differ.
    typedef typename HASHFUNC::type_key             LOOKUP_KEY;

protected:
    struct Node
    {
        VAL     _val;
        Node*   _next;

        Node() : _next(0) {}
    };


private:
    dynarray<Node*> _table;
    uints _nelem;

    typedef hashtable<VAL,LOOKUP_KEY,HASHFUNC,EQFUNC,GETKEYFUNC,ALLOC>  _Self;

protected:

    typedef hashtable<VAL,LOOKUP_KEY,HASHFUNC,EQFUNC,GETKEYFUNC,ALLOC>   _HT;

    void add( Node** n, const VAL& v )
    {
        Node* r = new Node;
        r->_next = *n;
        r->_val = v;
        *n = r;
    }

    HASHFUNC    _HASHFUNC;
    EQFUNC      _EQFUNC;
    GETKEYFUNC  _GETKEYFUNC;

    uints bucket( const LOOKUP_KEY& v ) const              { return _HASHFUNC(v) % _table.size(); }
    uints bucketn( const LOOKUP_KEY& v, uints n ) const    { return _HASHFUNC(v) % n; }


public:

    HASHFUNC hash_funct() const     { return _HASHFUNC; }
    EQFUNC key_eq() const           { return _EQFUNC; }


    class Ptr : public std::iterator<std::forward_iterator_tag, VAL>
    {
        typedef hashtable<VAL,LOOKUP_KEY,HASHFUNC,EQFUNC,GETKEYFUNC,ALLOC>   _HT;

        Node* _p;
        const _HT* _ht;

    public:

        typedef LOOKUP_KEY                    key_type;
        typedef VAL                    value_type;
        typedef HASHFUNC               hasher;
        typedef EQFUNC                 key_equal;

        typedef size_t                 size_type;
        typedef ptrdiff_t              difference_type;
        typedef VAL*                   pointer;
        typedef const VAL*             const_pointer;
        typedef VAL&                   reference;
        typedef const VAL&             const_reference;

        const Node* _get_node() const       { return _p; }
        const _HT* _get_ht() const          { return _ht; }

        Ptr( Node* p, const _HT& ht ) : _p(p), _ht(&ht) {}
        Ptr() {}

        bool operator == (const Ptr& p) const       { return _p == p._p; }
        bool operator != (const Ptr& p) const       { return _p != p._p; }

        reference operator*()                       { return _p->_val; }
        pointer operator ->()                       { return &_p->_val; }

        Ptr& operator++()
        {
            Node* n = _p->_next;
            if(!n)
                n = _ht->get_next(_p);
            _p = n;
            return *this;
        }
        
        inline Ptr operator++(int)
        {
            Ptr tmp = *this;
            ++*this;
            return tmp;
        } 
    };

    class CPtr : public std::iterator<std::forward_iterator_tag, VAL>
    {
        typedef hashtable<VAL,LOOKUP_KEY,HASHFUNC,EQFUNC,GETKEYFUNC,ALLOC>   _HT;

        const Node* _p;
        const _HT*  _ht;

    public:

        typedef LOOKUP_KEY                    key_type;
        typedef VAL                    value_type;
        typedef HASHFUNC               hasher;
        typedef EQFUNC                 key_equal;

        typedef size_t                 size_type;
        typedef ptrdiff_t              difference_type;
        typedef VAL*                   pointer;
        typedef const VAL*             const_pointer;
        typedef VAL&                   reference;
        typedef const VAL&             const_reference;

        CPtr( const Node* p, const _HT& ht ) : _p(p), _ht(&ht) {}
        CPtr() {}

        CPtr( const Ptr& p )
        {
            _p = p._get_node();
            _ht = p._get_ht();
        }

        CPtr& operator = (const CPtr& p)
        {
            _p = p._p;
            _ht = p._ht;
            return *this;
        }

        CPtr& operator = (const Ptr& p)
        {
            _p = p._get_node();
            _ht = p._get_ht();
            return *this;
        }

        bool operator == (const CPtr& p) const      { return _p == p._p; }
        bool operator != (const CPtr& p) const      { return _p != p._p; }

        const_reference operator*() const           { return _p->_val; }
        const_pointer operator ->()                 { return &_p->_val; }

        CPtr& operator++()
        {
            const Node* n = _p->_next;
            if(!n)
                n = _ht->get_next(_p);
            _p = n;
            return *this;
        }
        
        inline CPtr operator++(int)
        {
            CPtr tmp = *this;
            ++*this;
            return tmp;
        } 
    };

    typedef Ptr                                 iterator;
    typedef CPtr                                const_iterator;

protected:

    struct hashtable_binstream_container : public binstream_containerT<VAL>
    {
        virtual const void* extract( uints n )
        {
            DASSERT( _begin != _end );
            const VAL* p = &(*_begin);
            ++_begin;
            return p;
        }

        virtual void* insert( uints n )
        {
            Node* p = *_newnode.add() = new Node;
            return &p->_val;
        }

        virtual bool is_continuous() const      { return false; }


        hashtable_binstream_container( const _HT& ht )
            : binstream_containerT<VAL>(ht.size()), _ht((_HT&)ht)
        {
            _begin = _ht.begin();
            _end = _ht.end();
        }

        hashtable_binstream_container( _HT& ht, uints n )
            : binstream_containerT<VAL>(n), _ht(ht)
        {
            _begin = _ht.begin();
            _end = _ht.end();
        }

        ~hashtable_binstream_container()
        {
            for( uints i=0; i<_newnode.size(); ++i )
            {
                Node** ppn = _ht.find_socket( _ht._GETKEYFUNC(_newnode[i]->_val) );
                _newnode[i]->_next = *ppn;
                *ppn = _newnode[i];
            }

            _ht._nelem += _newnode.size();
        }

    protected:
        _HT& _ht;
        const_iterator _begin, _end;
        dynarray<Node*> _newnode;
    };


    ///Find first node that matches the key
    Node* find_node( const LOOKUP_KEY& k ) const
    {
        uints h = bucket(k);
        Node* n = (Node*)_table[h];
        while(n)
        {
            if( _EQFUNC( _GETKEYFUNC(n->_val), k ) )
                return n;
            n = n->_next;
        }
        return 0;
    }

    ///Find first node that matches the key
    Node** find_socket( const LOOKUP_KEY& k ) const
    {
        uints h = bucket(k);
        Node** n = (Node**)&_table[h];
        while(*n)
        {
            if( _EQFUNC( _GETKEYFUNC((*n)->_val), k ) )
                return n;
            n = &(*n)->_next;
        }
        return n;
    }

    ///Find first node that matches the key
    Node** find_socket( dynarray<Node*>& a, const LOOKUP_KEY& k ) const
    {
        uints h = bucketn( k, a.size() );
        Node** n = (Node**)&a[h];
        while(*n)
        {
            if( _EQFUNC( _GETKEYFUNC((*n)->_val), k ) )
                return n;
            n = &(*n)->_next;
        }
        return n;
    }

    ///Delete all records that match the key
    uints del( const LOOKUP_KEY& k )
    {
        Node** ppn = find_socket(k);
        Node* n = *ppn;
        if(!n)  return 0;

        uints c=0;
        while( n  &&  _EQFUNC( _GETKEYFUNC(n->_val), k ) )
        {
            Node* t = n->_next;
            delete n;
            n = t;
            ++c;
        }

        *ppn = n;
        _nelem -= c;
        return c;
    }

    ///Find first node that matches the key
    Node** get_socket( const Node* n ) const
    {
        if(!n)  return 0;
        uints h = bucket( _GETKEYFUNC(n->_val) );

        Node** pn = (Node**)&_table[h];
        while(*pn)
        {
            if( *pn == n )
                return pn;
            pn = &(*pn)->_next;
        }
        RASSERTX( 1, "invalid argument" );
        return 0;
    }

    ////////////////////////////////////////////////////////////////////////////////
public:

    binstream& stream_out( binstream& bin ) const
    {
        hashtable_binstream_container bc(*this);
        bin.xwrite_array( bc );

        return bin;
    }

    binstream& stream_in( binstream& bin )
    {
        hashtable_binstream_container bc(*this,UMAX);
        bin.xread_array( bc );

        return bin;
    }


    ///
    Node* get_next( const Node* cn ) const
    {
        if(!cn)  return 0;
        uints h = bucket( _GETKEYFUNC(cn->_val) );
        uints n = _table.size();
        DASSERTX( _table[h] != 0, "probably mixed keys and different hash functions used" );
        for( ++h; h<n; ++h )
        {
            if( _table[h] )
                return (Node*)_table[h];
        }
        return 0;
    }

    size_t size() const          { return _nelem; }
    size_t max_size() const      { return size_t(-1); }
    bool empty() const           { return size() == 0; }

    void swap(_Self& ht)
    {
        std::swap( _HASHFUNC, ht._HASHFUNC );
        std::swap( _EQFUNC, ht._EQFUNC );
        std::swap( _GETKEYFUNC, ht._GETKEYFUNC );
        _table.swap( ht._table );
        std::swap( _nelem, ht._nelem );
    }

    iterator begin()
    {
        uints n = _table.size();
        for( uints i=0; i<n; ++i )
            if( _table[i] )
                return iterator( _table[i], *this );
        return end();
    }

    iterator end()      { return iterator( (Node*)0, *this ); }

    const_iterator begin() const
    {
        uints n = _table.size();
        for( uints i=0; i<n; ++i )
            if( _table[i] )
                return const_iterator( _table[i], *this );
        return end();
    }

    const_iterator end() const          { return const_iterator( (Node*)0, *this ); }

  
    size_t bucket_count() const         { return _table.size(); }

    size_t max_bucket_count() const
    {   return size_t(-1);   } 

    size_t elems_in_bucket( size_t k ) const
    {
        Node* n = _table[k];
        size_t r=0;
        for( ; n!=0; n=n->_next )  ++r;
        return r;
    }


    iterator find( const LOOKUP_KEY& k )
    {
        Node* n = find_node(k);
        if(n)
            return iterator( n, *this );
        else
            return end();
    }

    const_iterator find( const LOOKUP_KEY& k ) const
    {
        const Node* n = find_node(k);
        if(n)
            return const_iterator( n, *this );
        else
            return end();
    }

    size_t count( const LOOKUP_KEY& k )
    {
        Node* n = find_node(k);
        if(!n)  return 0;
        
        size_t c=0;
        while( n  &&  _EQFUNC( _GETKEYFUNC(n->_val), k ) )
        {
            ++c;
            n = n->_next;
        }
        
        return c;
    }

    std::pair<iterator, iterator> equal_range( const LOOKUP_KEY& k )
    {
        Node* f = find_node(k);
        if(!f)
            return std::pair<iterator,iterator>( end(), end() );
        
        Node* l = f->_next;
        while( l  &&  _EQFUNC( _GETKEYFUNC(l->_val), k ) )
            l = l->_next;

        if(!l)
            l = get_next(l);
        return std::pair<iterator,iterator>( iterator(f,*this), iterator(l,*this) );
    }

    std::pair<const_iterator, const_iterator> equal_range( const LOOKUP_KEY& k ) const
    {
        const Node* f = find_node(k);
        if(!f)
            return std::pair<const_iterator,const_iterator>( end(), end() );
        
        const Node* l = f->_next;
        while( l  &&  _EQFUNC( _GETKEYFUNC(l->_val), k ) )
            l = l->_next;

        if(!l)
            l = get_next(l);
        return std::pair<const_iterator,const_iterator>( const_iterator(f,*this), const_iterator(l,*this) );
    }

    bool erase_value( const LOOKUP_KEY& k, VAL* dst )
    {
        Node** pn = find_socket(k);
        if(!*pn)
            return false;

        Node* n = *pn;
        *pn = n->_next;
        if(dst)
            *dst = n->_val;
        delete n;
        --_nelem;

        return true;
    }

    size_t erase( const LOOKUP_KEY& k )        { return del(k); }

    void erase( const_iterator& it )
    {
        Node** pn = get_socket(it._p);
        Node* n = *pn;
        *pn = n->_next;
        delete n;
        --_nelem;
    }

    void erase( const_iterator f, const_iterator l )
    {
        Node** pn = get_socket(f._p);
        Node* n = *pn;
        if(!n)  return;
        Node* nl = l._p;
        while( n  &&  n!=nl )
        {
            Node* t = n;
            n = n->_next;
            if(!n)
                n = get_next(t);
            delete t;
            --_nelem;
        }
    }


    void resize( size_t bucketn )
    {
        uints ts = _table.size();
        if( bucketn > ts )
        {
            dynarray<Node*> temp;
            temp.need_newc( nextpow2(bucketn) );

            for( uints i=0; i<ts; ++i )
            {
                Node* n = _table[i];
                while(n)
                {
                    Node* t = n->_next;
                    Node** pn = find_socket( temp, _GETKEYFUNC(n->_val) );
                    n->_next = *pn;
                    *pn = n;
                    n = t;
                }
            }

            temp.swap( _table );
        }
    }

    void clear()
    {
        for( uints i=0; i<_table.size(); ++i )
        {
            Node* n = _table[i];
            while(n)
            {
                Node* t = n->_next;
                delete n;
                n = t;
            }
        }

        _nelem = 0;
        _table.need_newc(64);
    }


    std::pair<iterator, bool> insert_unique( const VAL& v )
    {
        resize( _nelem + 1 );

        Node** ppn = _insert_unique(v);
        if(!ppn)
            return std::pair<iterator,bool>( end(), false );
        else
            return std::pair<iterator,bool>( iterator( *ppn, *this ), true );
    }

    iterator insert_equal( const VAL& v )
    {
        resize( _nelem + 1 );
        return iterator( *_insert_equal(v), *this );
    }

    void insert_unique( const VAL* f, const VAL* l )
    {
        size_t n = l - f;
        resize( _nelem + n );
        for( ; n>0; --n, ++f )
            _insert_unique(*f);
    }

    void insert_equal( const VAL* f, const VAL* l )
    {
        size_t n = l - f;
        resize( _nelem + n );
        for( ; n>0; --n, ++f )
            _insert_equal(*f);
    }

    void insert_unique( const_iterator f, const_iterator l )
    {
        size_t n = 0;
        std::distance( f, l, n );
        resize( _nelem + n );
        for( ; n>0; --n, ++f )
            _insert_unique(*f);
    }

    void insert_equal( const_iterator f, const_iterator l )
    {
        size_t n = 0;
        std::distance( f, l, n );
        resize( _nelem + n );
        for( ; n>0; --n, ++f )
            _insert_equal(*f);
    }
    
 
    hashtable( uints n, const HASHFUNC& hf, const EQFUNC& eqf, const GETKEYFUNC& gkf )
    :
        _HASHFUNC(hf),
        _EQFUNC(eqf),
        _GETKEYFUNC(gkf)
    {
        _nelem = 0;
        _table.need_newc(64);
    }
/*
    hashtable( uints n, const HASHFUNC& hf, const EQFUNC& eqf )
    :
        _HASHFUNC(hf),
        _EQFUNC(eqf),
        _GETKEYFUNC( _Select_Itself<VAL>() )
    {
        _table.need_newc(64);
    }
*/
    hashtable( const hashtable& ht )
    :
        _HASHFUNC( ht._HASHFUNC ),
        _EQFUNC( ht._EQFUNC ),
        _GETKEYFUNC( ht._GETKEYFUNC )
    {
        copy_from(ht);
    }

    _Self& operator= (const _Self& ht)
    {
        clear();
        copy_from(ht);
        return *this;
    }

    ~hashtable()    { clear(); } 

private:
    void copy_from( const _Self& ht )
    {
        uints n = ht._table.size();
        _table.need_newc( ht._table.size() );

        for( uints h=0; h<n; ++h )
        {
            Node** pn = &_table[h];
            const Node* cn = ht._table[h];
            while(cn)
            {
                Node* n = new Node(*cn);
                *pn = n;
                pn = &n->_next;
                cn = cn->_next;
            }
            *pn = 0;
        }

        _nelem = ht._nelem;
    }

protected:
    Node** _insert_unique( const VAL& v )
    {
        typename GETKEYFUNC::retType k = _GETKEYFUNC(v);
        Node** ppn = find_socket(k);
        if( *ppn == 0  ||  !_EQFUNC( _GETKEYFUNC((*ppn)->_val), k ) )
        {
            Node* n = new Node;
            n->_next = *ppn;
            n->_val = v;
            *ppn = n;

            ++_nelem;
            return ppn;
        }

        return 0;
    }

    Node** _insert_unique_slot( const LOOKUP_KEY& k )
    {
        Node** ppn = find_socket(k);
        if( *ppn == 0  ||  !_EQFUNC( _GETKEYFUNC((*ppn)->_val), k ) )
        {
            Node* n = new Node;
            n->_next = *ppn;
            *ppn = n;

            ++_nelem;
            return ppn;
        }

        return 0;
    }

    Node** _insert_unique__replace( const VAL& v )
    {
        typename GETKEYFUNC::retType k = _GETKEYFUNC(v);
        Node** ppn = find_socket(k);
        if( *ppn != 0  &&  _EQFUNC( _GETKEYFUNC((*ppn)->_val), k ) )
        {
            Node* n = *ppn;
            *ppn = n->_next;
            delete n;
            --_nelem;
        }

        Node* n = new Node;
        n->_next = *ppn;
        n->_val = v;
        *ppn = n;

        ++_nelem;
        return ppn;
    }

    Node** _insert_equal( const VAL& v )
    {
        typename GETKEYFUNC::retType k = _GETKEYFUNC(v);
        Node** ppn = find_socket(k);

        Node* n = new Node;
        n->_next = *ppn;
        n->_val = v;
        *ppn = n;

        ++_nelem;
        return ppn;
    }

    Node** _insert_equal_slot( const LOOKUP_KEY& k )
    {
        Node** ppn = find_socket(k);

        Node* n = new Node;
        n->_next = *ppn;
        *ppn = n;

        ++_nelem;
        return ppn;
    }
};


COID_NAMESPACE_END

#endif //__COID_COMM_HASHTABLE__HEADER_FILE__
