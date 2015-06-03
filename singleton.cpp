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

#include "singleton.h"
#include "sync/mutex.h"
#include "hash/hashkeyset.h"

#include "binstream/filestream.h"
#include "binstream/txtstream.h"

namespace coid {

void memtrack_shutdown();


////////////////////////////////////////////////////////////////////////////////
///Global singleton registrator
struct global_singleton_manager
{
    global_singleton_manager() : mx(500, false)
    {
        last = 0;
        count = 0;
        shutting_down = false;
    }

    void add( void* ptr, void (*fn_destroy)(void*), const char* type, const char* file, int line )
    {
        RASSERT( !shutting_down );

        comm_mutex_guard<_comm_mutex> mxg(mx);

        killer* k = new killer(ptr, fn_destroy, type, file, line);
        k->next = last;

        last = k;
        ++count;
    }

    void destroy()
    {
        uint n = count;

		if(last) {
#ifdef _DEBUG
            memtrack_shutdown();

			bofstream bof("singleton.log");
			txtstream tof(bof);
#endif

            shutting_down = true;

			while(last) {
				killer* tmp = last->next;

#ifdef _DEBUG
				tof << (n--) << " destroying '" << last->type << "' singleton created at "
					<< last->file << ":" << last->line << "\r\n";
				tof.flush();
#endif

				last->destroy();
				delete last;

				last = tmp;
			}
		}
    }

    ~global_singleton_manager() {
        destroy();
    }

    static global_singleton_manager& get();

private:
    struct killer {
        void* ptr;
        void (*fn_destroy)(void*);
        const char* type;
        const char* file;
        int line;

        killer* next;

        void destroy() {
            fn_destroy(ptr);
        }

        killer( void* ptr, void (*fn_destroy)(void*), const char* type, const char* file, int line )
            : ptr(ptr), fn_destroy(fn_destroy), type(type), file(file), line(line)
        {}
    };

    _comm_mutex mx;
    killer* last;

    uint count;

    bool shutting_down;
};


////////////////////////////////////////////////////////////////////////////////
void* singleton_register_instance( void* p, void (*fn_destroy)(void*),
    const char* type, const char* file, int line )
{
    auto& gsm = global_singleton_manager::get();

    gsm.add(p, fn_destroy, type, file, line);
    return p;
}

////////////////////////////////////////////////////////////////////////////////
void singletons_destroy()
{
    auto& gsm = global_singleton_manager::get();

    memtrack_shutdown();
    gsm.destroy();
}




////////////////////////////////////////////////////////////////////////////////
//code for process-wide singletons

#define GLOBAL_SINGLETON_REGISTRAR sglo_registrar

#ifdef SYSTYPE_WIN
typedef int (__stdcall *proc_t)();

extern "C"
__declspec(dllimport) proc_t __stdcall GetProcAddress (
    void* hmodule,
    const char* procname
    );

typedef global_singleton_manager* (*ireg_t)();

#define MAKESTR(x) STR(x)
#define STR(x) #x

extern "C" __declspec(dllexport) global_singleton_manager* GLOBAL_SINGLETON_REGISTRAR()
{
    static global_singleton_manager _gsm;
    return &_gsm;
}

global_singleton_manager& global_singleton_manager::get()
{
    static global_singleton_manager* _this=0;

    if(!_this) {
        //retrieve process-wide singleton from exported fn
        const char* s = MAKESTR(GLOBAL_SINGLETON_REGISTRAR);
        ireg_t p = (ireg_t)GetProcAddress(0, s);
        if(!p) {
            //entry point for global singleton not found in exe
            //probably a 3rd party exe
            _this = GLOBAL_SINGLETON_REGISTRAR();
        }
        else
            _this = p();
    }

    return *_this;
}

#else
/*
extern "C" __attribute__ ((visibility("default"))) interface_register_impl* INTERGEN_GLOBAL_REGISTRAR();
{
    return &SINGLETON(interface_register_impl);
}*/

global_singleton_manager& global_singleton_manager::get()
{
    static global_singleton_manager _this;

    return _this;
}

#endif

} //namespace coid
