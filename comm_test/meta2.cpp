
#include <comm/metastream/metastream.h>
#include <comm/metastream/fmtstreamjson.h>
#include <comm/hash/hashkeyset.h>
#include <comm/binstream/binstream.h>
#include <comm/binstream/filestream.h>
#include <comm/str.h>



class device_mapping
{
protected:
    coid::charstr _devicename;	    //< source raw input device (0x10:Kbd,0x20:Mouse,0x30:Joy)
	coid::charstr _eventname;	    //< device event name "KBD_A","KBD_UP","MOUSE_XY","MOUSE_BTN0","JOY_AXISX","JOY_AXISXY","JOY_BTN0" etc.
	coid::charstr _modifiername;	//< modifier name "KBD_ALT" "KBD_SHIFT" "KBD_CTRL" etc.
	bool _invert;				    //< invert input value (!state,-value,on ET_AXISXY invert only Y part)
	bool _pressed;				    //< send key event only on pressed state
	bool _continuos;			    //< send key event every frame if button is pressed

public:

	friend coid::binstream& operator << (coid::binstream& bin,const device_mapping& e) {   
		return bin
			<<e._devicename
			<<e._eventname
			<<e._invert
			<<e._pressed
			<<e._continuos
            <<e._modifiername
			;
	}

	friend coid::binstream& operator >> (coid::binstream& bin,device_mapping& e) {   
		return bin 
			>>e._devicename
			>>e._eventname
			>>e._invert
			>>e._pressed
			>>e._continuos
            >>e._modifiername
			;
	}

	friend coid::metastream& operator << (coid::metastream& m, const device_mapping& e) {
		MSTRUCT_OPEN(m,"device_mapping")
			MM(m,"device_name",e._devicename)
			MM(m,"event_name",e._eventname)
			MMD(m,"invert",e._invert,false)
			MMD(m,"pressed",e._pressed,false)
			MMD(m,"continuos",e._continuos,false)
            MMD(m,"modifiers",e._modifiername,coid::charstr())
		MSTRUCT_CLOSE(m)
	}
};




class event_source_mapping
{
protected:
	coid::charstr _name;		    //< event name
    coid::charstr _desc;            //< event description

    typedef coid::dynarray<device_mapping> device_mapping_list_t;

    device_mapping_list_t _mappings; //< all mappings to this event

public:

    operator const coid::token () const { return _name; }

    friend coid::binstream& operator << (coid::binstream& bin,const event_source_mapping& e) {   
		return bin
			<<e._name
            <<e._desc
            <<e._mappings
			;
	}

	friend coid::binstream& operator >> (coid::binstream& bin,event_source_mapping& e) {   
		return bin 
			>>e._name
            >>e._desc
            >>e._mappings
			;
	}

	friend coid::metastream& operator << (coid::metastream& m, const event_source_mapping& e) {
		MSTRUCT_OPEN(m,"event_src")
			MM(m,"name",e._name)
            MMD(m,"desc",e._desc,coid::charstr())
            MMAT(m,"mappings",device_mapping)
		MSTRUCT_CLOSE(m)
	}
};



class io_man
{
	// loaded event source mappings
	typedef coid::hash_keyset<event_source_mapping,coid::_Select_Copy<event_source_mapping,coid::token>> event_source_map_t;

	event_source_map_t _map;

public:


	friend coid::binstream& operator << (coid::binstream& bin,const io_man& iom) {
		return bin<<iom._map;
	}

	friend coid::binstream& operator >> (coid::binstream& bin,io_man& iom) {   
		return bin>>iom._map;
	}

	friend coid::metastream& operator << (coid::metastream& m, const io_man& iom) {
		MSTRUCT_OPEN(m,"io_man")
			MMAT(m,"io_src_map",event_source_mapping)
		MSTRUCT_CLOSE(m)
	}
};

using namespace coid;

void metastream_test2()
{
    bifstream bif("iomap.cfg");
    fmtstreamjson fmt(bif);
    metastream meta(fmt);

    io_man b;
    meta.stream_in(b);
    meta.stream_acknowledge();
}