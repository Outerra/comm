#pragma once

#include "slotalloc.h"
#include <type_traits>

COID_NAMESPACE_BEGIN

///Helper class for key extracting from values, performing a cast
template<class T, class KEY>
struct extractor
{
    KEY operator()( const T& value ) const { return value; }
};

template<class T, class KEY>
struct extractor<T*, KEY*>
{
    KEY* operator()(T* value) const { return value; }
};

template<class T, class KEY>
struct extractor<T*, KEY>
{
    KEY operator()(T* value) const { return *value; }
};


////////////////////////////////////////////////////////////////////////////////
///Hash map (keyset) based on the slotalloc container, providing items that can be accessed
/// both with an id or a key.
///Items in the container are assumed to store the key within themselves, and able to extract
/// it via an extractor
template<
    class T,
    class KEY,
    class EXTRACTOR = extractor<T, KEY>,
    class HASHFUNC = hash<KEY>,
    bool POOL=false
>
class slotalloc_hash : public slotalloc<T, POOL>
{
public:

    T* push( const T& v ) = delete;
    T* add() = delete;
    T* add_uninit( bool* newitem = 0 ) = delete;

    slotalloc_hash()
    {
        //append related array for hash table sequences
        append_relarray<uint>(&_seqtable);

        _buckets.calloc(32, true);
    }

    const T* find_value( const KEY& key ) const
    {
        uint b = bucket(key);
        uint id = find_object(b, key);

        return id != UMAX32 ? get_item(id) : 0;
    }

    T* find_or_insert_value_slot( const KEY& key, bool* isnew=0 )
    {
        uint id;
        if(find_or_insert_value_slot_uninit_(key, &id)) {
            if(isnew) *isnew = true;
            return new(get_item(id)) T;
        }
        
        return get_item(id);
    }

    T* find_or_insert_value_slot_uninit( const KEY& key, bool* isnew=0 )
    {
        uint id;
        bool isnew_ = find_or_insert_value_slot_uninit_(key, &id);
        if(isnew)
            *isnew = isnew_;

        return get_item(id);
    }

    T* insert_value_slot( const KEY& key ) { return insert_value_(add(), key); }

    T* insert_value_slot_uninit( const KEY& key ) { return insert_value_(add_uninit(), key); }

    ///Delete item from hash map
    void del( T* p )
    {
        uint id = (uint)get_item_id(p);

        const KEY& key = _EXTRACTOR(*p);
        uint b = bucket(key);

        //remove id from the bucket list
        uint* n = find_object_entry(b, key);
        DASSERT( *n == id );

        *n = (*_seqtable)[id];

        slotalloc::del(p);
    }


    ///Delete all items that match the key
    uints erase( const KEY& key )
    {
        uint b = bucket(key);
        uint c = 0;

        uint* n = find_object_entry(b, key);
        while(*n != UMAX32) {
            if(!(_EXTRACTOR(*get_item(*n)) == key))
                break;

            uint id = *n;
            *n = (*_seqtable)[id];

            slotalloc::del(get_item(id));
            ++c;
        }

        return c;
    }

protected:

    uint bucket( const KEY& k ) const       { return uint(_HASHFUNC(k) % _buckets.size()); }

    ///Find first node that matches the key
    uint find_object( uint bucket, const KEY& k ) const
    {
        uint n = _buckets[bucket];
        while(n != UMAX32)
        {
            if(_EXTRACTOR(*get_item(n)) == k)
                return n;
            n = (*_seqtable)[n];
        }

        return n;
    }

    uint* find_object_entry( uint bucket, const KEY& k )
    {
        uint* n = &_buckets[bucket];
        while(*n != UMAX32)
        {
            if(_EXTRACTOR(*get_item(*n)) == k)
                return n;
            n = &(*_seqtable)[*n];
        }

        return n;
    }

    bool find_or_insert_value_slot_uninit_( const KEY& key, uint* pid )
    {
        uint b = bucket(key);
        uint id = find_object(b, key);

        bool isnew = id == UMAX32;
        if(isnew) {
            T* p = slotalloc::add_uninit();
            id = (uint)get_item_id(p);

            (*_seqtable)[id] = _buckets[b];
            _buckets[b] = id;
        }

        *pid = id;
        
        return isnew;
    }

    T* insert_value_( T* p )
    {
        uint id = (uint)get_item_id(p);

        uint b = bucket(key);
        uint fid = find_object(b, key);

        _seqtable[id] = fid != UMAX32 ? fid : _buckets[b];

        if(_buckets[b] == _seqtable[id])
            _buckets[b] = id;

        return p;
    }

private:

    EXTRACTOR _EXTRACTOR;
    HASHFUNC _HASHFUNC;

    coid::dynarray<uint> _buckets;      //< table with ids of first objects belonging to the given hash socket
    coid::dynarray<uint>* _seqtable;    //< table with ids pointing to the next object in hash socket chain
};

COID_NAMESPACE_END
