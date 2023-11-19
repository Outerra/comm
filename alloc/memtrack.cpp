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
 * Brano Kemen
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "memtrack.h"
#include "memtrack_stacktrace.h"
#include "../hash/hashkeyset.h"
#include "../singleton.h"
#include "../atomic/atomic.h"
#include "../sync/mutex.h"

#include "../binstream/filestream.h"

#ifdef _DEBUG
static const bool default_enabled = true;
#else
static const bool default_enabled = false;
#endif


namespace coid {


static void name_filter(charstr& dst, token name)
{
    //remove struct, class
    static const token _struct = "struct";
    static const token _class = "class";
    static const token _array = "[0]";

    do {
        token x = name.cut_left(' ');

        if (x.ends_with(_struct))
            x.shift_end(-int(_struct.len()));
        else if (x.ends_with(_class))
            x.shift_end(-int(_class.len()));
        else if (x == _array) {
            dst.append("[]");
            x.set_empty();
        }
        else if (name)
            x.shift_end(1); //give space back

        dst.append(x);
    } while (name);
}

/////
//struct memtrack_imp : memtrack {
//    bool operator == (size_t k) const {
//        return (size_t)name == k;
//    }
//
//    operator size_t() const {
//        return (size_t)name;
//    }
//
//    //don't track this
//    void* operator new(size_t size) { return ::dlmalloc(size); }
//    void operator delete(void* ptr) { ::dlfree(ptr); } \
//};
//
/////
//struct hash_memtrack {
//    typedef size_t key_type;
//    uint operator()(size_t x) const { return (uint)x; }
//};

struct memtrack_key_extractor {
    using ret_type = uint;
    ret_type  operator()(const memtrack& m) const { return m.hash; }
};

/// @brief  dummy hasher just return uint as it as because memtrack key is already hash
struct memtrack_key_hasher
{
    using key_type = uint;
    uint operator()(const key_type& key) const { return key; }

};

typedef hash_keyset<memtrack, memtrack_key_extractor, memtrack_key_hasher>
memtrack_hash_t;

///
struct memtrack_registrar
{
    inline static bool inside_alloc_in_progress = false;

    volatile bool running = false;

    memtrack_hash_t* hash = 0;
    comm_mutex* mux = 0;

    coid::dynarray32<memtrack_stacktrace> stacktrace_records;
    comm_mutex* stacktrace_mux = 0;

    bool enabled = default_enabled;
    bool ready = false;
    bool enable_stacktrace_recording = false;

    memtrack_registrar()
    {
        mux = new comm_mutex(500, false);
        hash = new memtrack_hash_t;
        
        stacktrace_mux = new comm_mutex(500, false);

        ready = true;
    }

    virtual ~memtrack_registrar() {}

    /// @note virtual methods to avoid breaking dlls when exe implementation changes

private: // these are depredicated methods after refactor from std::type_info to coid::type_info
    // to keed the binary compability with dlls compiled with older comm version
    // DO NOT MOVE!!!
    virtual void alloc_depredicated(const std::type_info* tracking, size_t size)
    {
    }

    virtual void free_depredicated(const std::type_info* tracking, size_t size)
    {
    }

    virtual uint list_depredicated(memtrack* dst, uint nmax, bool modified_only) const
    {
        return 0;
    }

public:
    virtual void dump(const char* file, bool diff) const
    {
        GUARDTHIS(*mux);
        memtrack_hash_t::iterator ib = hash->begin();
        memtrack_hash_t::iterator ie = hash->end();

        bofstream bof(file);
        if (!bof.is_open())
            return;

        LOCAL_SINGLETON(charstr) dumpbuf;
        charstr& buf = *dumpbuf;

        buf.reserve(8000);
        buf.reset();

        buf << "======== bytes | #alloc |  type ======\n";

        int64 totalsize = 0;
        size_t totalcount = 0;

        for (; ib != ie; ++ib) {
            memtrack& p = *ib;
            if (diff ? (p.size == 0) : (p.cursize == 0))
                continue;

            ints size = diff ? p.size : ints(p.cursize);
            uint count = diff ? p.nallocs : p.ncurallocs;

            totalsize += size;
            totalcount += count;

            buf.append_num_thousands(size, ',', 12);
            buf.append_num(10, count, 9);
            buf << '\t';
            name_filter(buf, p.name);
            buf << '\n';

            if (buf.len() > 7900) {
                bof.xwrite_token_raw(buf);
                buf.reset();
            }
        }

        buf << "======== bytes | #alloc |  type ======\n";
        buf.append_num_metric(totalsize, 14);
        buf << 'B';
        buf.append_num_thousands(totalcount, ',', 8);
        buf << "\t (total)\n";

        mallinfo ma = mspace_mallinfo(SINGLETON(comm_array_mspace).msp);
        mallinfo md = dlmallinfo();

        buf << "\nnon-mmapped space allocated from system: " << num_metric(md.arena + ma.arena, 8);    buf << 'B';
        buf << "\nnumber of free chunks:                   " << num_metric(md.ordblks + ma.ordblks, 8);
        buf << "\nmaximum total allocated space:           " << num_metric(md.usmblks + ma.usmblks, 8);  buf << 'B';
        buf << "\ntotal allocated space:                   " << num_metric(md.uordblks + ma.uordblks, 8); buf << 'B';
        buf << "\ntotal free space:                        " << num_metric(md.fordblks + ma.fordblks, 8); buf << 'B';
        buf << "\nreleasable (via malloc_trim) space:      " << num_metric(md.keepcost + ma.keepcost, 8); buf << 'B';

        bof.xwrite_token_raw(buf);
        bof.close();
    }

    virtual uint count() const {
        GUARDTHIS(*mux);
        return (uint)hash->size();
    }

    virtual void reset() {
        {
            GUARDTHIS(*mux);
            memtrack_hash_t::iterator ib = hash->begin();
            memtrack_hash_t::iterator ie = hash->end();

            for (; ib != ie; ++ib) {
                memtrack& p = *ib;
                p.nallocs = 0;
                p.size = 0;
            }
        }
        
        {
            GUARDTHIS(*stacktrace_mux);
            stacktrace_records.reset();
        }
    }

    ///Track allocation
    virtual void alloc(const coid::type_info* tracking, size_t size)
    {
        if (inside_alloc_in_progress)
            return;     //avoid stack overlow from hashmap and stacktrace add

        constexpr token_literal unknown = "unknown"_T;
        constexpr uint unknown_hash = unknown.hash();

        GUARDTHIS(*mux);
        inside_alloc_in_progress = true;
        memtrack* val = hash->find_or_insert_value_slot(
            tracking != nullptr ? tracking->hash : unknown_hash
        );

        val->name = tracking != nullptr ? tracking->name : unknown;
        val->hash = tracking != nullptr ? tracking->hash : unknown_hash;

        ++val->nallocs;
        ++val->ncurallocs;
        ++val->nlifeallocs;
        val->size += size;
        val->cursize += size;
        val->lifesize += size;

        if (enable_stacktrace_recording)
        {
            GUARDTHIS(*stacktrace_mux);

            memtrack_stacktrace* memtrack_trace_ptr = stacktrace_records.add();
            memtrack_trace_ptr->_name = val->name;
            memtrack_trace_ptr->_size = size;
            memtrack_trace_ptr->_stack_trace = stacktrace::get_current_stack_trace();
        }

        inside_alloc_in_progress = false;
    }

    ///Track freeing
    virtual void free(const coid::type_info* tracking, size_t size)
    {
        constexpr token_literal unknown = "unknown"_T;
        constexpr uint unknown_hash = unknown.hash();

        GUARDTHIS(*mux);
        if (inside_alloc_in_progress) // this must be from realloc of stacktrace_records from alloc method so don't record it
        {
            return;
        }

        memtrack* val = const_cast<memtrack*>(hash->find_value(
            tracking != nullptr ? tracking->hash : unknown_hash
        ));

        if (val) {
            //val->size -= size;
            val->cursize -= size;
            --val->ncurallocs;
        }
    }

    virtual uint list(memtrack* dst, uint nmax, bool modified_only) const
    {
        GUARDTHIS(*mux);
        memtrack_hash_t::iterator ib = hash->begin();
        memtrack_hash_t::iterator ie = hash->end();

        uint i = 0;
        for (; ib != ie && i < nmax; ++ib) {
            memtrack& p = *ib;
            if (p.nallocs == 0 && modified_only)
                continue;

            dst[i++] = p;
            p.nallocs = 0;
            p.size = 0;
        }

        return i;
    }
};

////////////////////////////////////////////////////////////////////////////////
static memtrack_registrar* memtrack_register()
{
    static bool reentry = false;
    if (reentry)
        return 0;

    reentry = true;
    LOCAL_FUNCTION_PROCWIDE_SINGLETON_DEF(memtrack_registrar) reg;
    reentry = false;

    return reg.get();
}

////////////////////////////////////////////////////////////////////////////////
bool memtrack_enable(bool en)
{
    memtrack_registrar* mtr = memtrack_register();
    bool old = mtr->enabled;

    mtr->enabled = en;
    mtr->running = en & mtr->ready;

    return old;
}

////////////////////////////////////////////////////////////////////////////////
void memtrack_shutdown()
{
    memtrack_registrar* mtr = memtrack_register();
    mtr->running = mtr->enabled = mtr->ready = false;
}

////////////////////////////////////////////////////////////////////////////////
void memtrack_alloc(const coid::type_info* tracking, size_t size)
{
    memtrack_registrar* mtr = memtrack_register();
    if (!mtr || !mtr->running) return;

    mtr->alloc(tracking, size);
}

////////////////////////////////////////////////////////////////////////////////
void memtrack_free(const coid::type_info* tracking, size_t size)
{
    memtrack_registrar* mtr = memtrack_register();
    if (!mtr || !mtr->running) return;

    mtr->free(tracking, size);
}

////////////////////////////////////////////////////////////////////////////////
uint memtrack_list(memtrack* dst, uint nmax, bool modified_only)
{
    memtrack_registrar* mtr = memtrack_register();
    if (!mtr || !mtr->ready)
        return 0;

    return mtr->list(dst, nmax, modified_only);
}

////////////////////////////////////////////////////////////////////////////////
void memtrack_dump(const char* file, bool diff)
{
    memtrack_registrar* mtr = memtrack_register();
    if (!mtr || !mtr->ready)
        return;

    mtr->dump(file, diff);
}

////////////////////////////////////////////////////////////////////////////////
uint memtrack_count()
{
    memtrack_registrar* mtr = memtrack_register();
    if (!mtr) return 0;

    return mtr->count();
}

////////////////////////////////////////////////////////////////////////////////
void memtrack_reset()
{
    memtrack_registrar* mtr = memtrack_register();

    mtr->reset();
}
////////////////////////////////////////////////////////////////////////////////
void enable_record_memtrack_stacktrace(bool enable)
{
    memtrack_registrar* mtr = memtrack_register();
    if (!mtr || !mtr->ready)
        return;

    mtr->enable_stacktrace_recording = enable;
}

////////////////////////////////////////////////////////////////////////////////
uint memtrack_stacktrace_list(memtrack_stacktrace* destionaion, uint max_count)
{
    memtrack_registrar* mtr = memtrack_register();
    if (!mtr || !mtr->ready)
        return 0;

    GUARDTHIS(*mtr->stacktrace_mux);

    const uint size = mtr->stacktrace_records.size();
    for (uint i = 0; i < size && i < max_count; ++i)
    {
        *destionaion = std::move(mtr->stacktrace_records[i]);
        ++destionaion;
    }

    if (max_count < size)
    {
        mtr->stacktrace_records.del(0, max_count);
        return max_count;
    }
    else 
    {
        mtr->stacktrace_records.reset();
        return size;
    }
}

////////////////////////////////////////////////////////////////////////////////
uint memtrack_stacktrace_count()
{
    memtrack_registrar* mtr = memtrack_register();
    if (!mtr || !mtr->ready)
        return 0;

    GUARDTHIS(*mtr->stacktrace_mux);
    return mtr->stacktrace_records.size();
}

} //namespace coid
