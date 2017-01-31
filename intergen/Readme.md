# INTERGEN

This document describes the intergen tool for generating APIs from C++ classes. The naming convention used here:

**host class** - an internal application class, can expose multiple interfaces to the world

**client interface class** - generated class for a particular interface

An example of how the host class is decorated for intergen to automatically create both C++ and JavaScript client interfaces:

```c++
    //host class
    class engine
    {
    public:
        //declare an interface, declared name is used as the interface class name
        // the name can optionally contain a namespace under which it should be created
        //the second parameter is a relative path to the generated interface header,
        // with optional interface file name
        ifc_class(ot::engine_interface, ”ifc/”)
    
        //non-static interface methods
        ifc_fn bool create_entity( const char* name, ifc_out uint* eid );
        ifc_fn void delete_entity( uint eid );
    
        //in case you want the interface method to be named differently than the method
        // name in the host class, use ifc_fnx(name) to declare it
        ifc_fnx(something) int do_something();
    
        //an interface creator static method (see below)
        ifc_fn static iref<engine> get( const char* param );
    
        // other normal class content ...
    };
```

Running the intergen tool on the header file containing this class definition generates an interface class containing the decorated methods from the host class.

```c++
    namespace ot {
    
    //interface class
    class engine_interface
    {
    public:
        //a static method to retrieve the interface
        static iref<engine_interface> get( const char* param );
    
        bool create_entity( const char* name, uint* eid );
        void delete_entity( uint eid );
    
        int something();
    };
    } //namespace ot
```

The generated client interface class can be used like this:

```c++
    //usage: create interface object by calling its creator method that internally calls
    // host interface creator method
    iref<engine_interface> ent = engine_interface::get(“blabla”);
    
    //call a method of the interface
    uint eid;
    ent->create_entity(“hoohoo”, &eid);
```
    
Note that one host class can have multiple interfaces defined.





### Interface methods

Normal methods callable from the interface can be simply declared by prefixing the method with `ifc_fn` or `ifc_fnx` keyword. Additionally, any out and in-out parameters have to be decorated by `ifc_out` or `ifc_inout` prefix keywords, respectively.

```c++
    //host class
    class engine
    {
    public:
        ifc_class(ot::engine_interface, ”ifc/”)
    
        //an interface creator static method
        ifc_fn static iref<engine> get( const char* param );
    
        //non-static interface methods, out/inout parameters prefixed
        ifc_fn bool create_entity( const char* name, ifc_out uint* eid );
        ifc_fn void delete_entity( uint eid );
    
        //enum type parameters must have explicit enum keyword
        ifc_fn bool something( enum ESomething p );
    };
```    

Enum parameter types need to specify the enum keyword so that intergen can generate proper type casts internally.


Interface destructor can be propagated as a special method of the host class, decorated by `ifc_fnx(~)`. This method is not exposed in the interface directly, it gets called only when the interface is released.

```c++
    //host class
    class engine
    {
    public:
        ifc_class(ot::engine_interface, ”ifc/”)

        //a method called when interface client is released
        ifc_fnx(~) void destroy();
```


### Obtaining interfaces

Interface methods have the same parameter and return value types as the corresponding host class methods, except for the static methods returning iref to the host class (note: other static methods aren’t allowed in interfaces).
These are used to obtain reference to the host object, that on the interface side translates into the interface itself.

```c++
    class engine
    {
    public:
        ifc_class(ot::engine_interface, ”ifc/”)

        //a static methods returning a reference to the host class are considered
        // to be the accessors used to obtain the host object when interface is requested.
        //a client would call these methods like engine_interface::get() to create
        // the interface, which will invoke these methods

        ifc_fn static iref<engine> get( const char* param );
    };
```

Note that if the host class is in a namespace, you have to use a fully qualified name when declaring the interface creator methods:

```c++
    namespace internal {
    class engine
    {
    public:
        ifc_class(ot::engine_interface, ”ifc/”)

        //use fully qualified names with namespaces as the return types
        ifc_fn static iref<internal::engine> get2( const char* param );
    };
    }
```

In this example the client class will be defined in the “ifc/engine_interface.h” header file, generated by intergen. Apart from this file the generator also produces a cpp file with the interface dispatcher. If the original header file where the host class was defined was “test.hpp”, then the generated dispatcher file will be named “test.intergen.cpp”.

For javascript clients there are extra parameters in each creator method added by the intergen, so that one can specify the context where the script runs. The script_handle argument can be set up with a script string, or a *.js script file name, or even a html file or url of page that should be open and that will have bound the reference to the interface to a symbol name specified as the last parameter of the javascript creator function.


### Arbitrary property access

To map property access in Javascript to get/set methods in C++ one can decorate operator() in C++ class with `ifc_fn` to get it function as property getter/setter for Javascript variable bound to an interface instance.

Two operators have to be provided: a setter with key and value-type arguments as a non-const method, and a getter returning the value-type, with one key-type parameter, as a const method.

```c++
    class engine
    {
    public:
        ifc_class(ot::engine_interface, ”ifc/”)
    
        ifc_fn double operator()(const char* key) const;
        ifc_fn void operator()(const char* key, double val);
    };
```

This will allow you to use arbitrary member access in the script, for example:

```c++
    ifc.key
    ifc[“a string key”]
```

Note: the key argument can be anything with implicit conversion from const char* type, or coid::token or coid::charstr types.




### Bidirectional interfaces

With bidirectional interfaces, instances of host class can talk back to the connected clients via events. Events are class methods that are only declared inside the host class definition, and their definition is generated by intergen in the corresponding intergen.cpp file.

Bidirectional interfaces can be declared via `ifc_class_var` macro, which also injects a member variable that will hold a reference to the connected client:

```c++
    //host class
    class engine
    {
    public:
        ifc_class_var(ot::engine_interface, ”ifc/”, var)

        //declaration only, the definition is generated automatically by intergen
        ifc_event void update( float dt );

        //use ifc_eventx(name) to have the interface event name different from
        // the name of the method in the host class
        ifc_eventx(init) void initialize();
    };
```
    
Host instance can detect whether a client is connected by testing the value of variable declared with `ifc_class_var` clause (var in the above example). 

`ifc_event` can be used to decorate a method that can be called from host class and will be invoked in the derived client class.


### Interface inheritance

Interface can be declared virtual using `ifc_class_virtual` decoration:

```c++
    ifc_class_virtual(ot::object, "ifc/");
    
    //normal ifc_fn/ifc_event methods
```
A virtual interface doesn't have creators, and can be used to provide an abstract base for other derived interfaces. A derived interface must implement all base interface methods.
To declare a derived interface, use the following syntax:

```c++
    ifc_class_var(ot::derived : ot::base, "ifc/")
    
    //declare all base methods + any extra ones
```

### Additional constructs recognized by intergen

The intergen parser recognizes the following constructs that help in controlling what should be included in the generated interface header files:

```c++
    //ifc{
      … code that should be shared with the interface class, will be copied into the
        generated header file …
    //}ifc

    /*ifc{
      … code that will be copied into the interface class header file, but is
        inaccessible in the host class …
    }ifc*/

    class engine
    {
        ...
```

To inject a code block specifically into the header class of a particular interface header, use the following construct:

```c++
    //ifc{ ot::engine_interface
      … code that should be shared with the interface class, will be copied into the
        generated header file of ot::engine_interface only …
    //}ifc
```

`ifc_fnx(!)` or `ifc_eventx(!)` can be used to suppress generating the method or the event in the scripting interface.






## Javascript interface

Apart from the C++ interface class, intergen also generates Javascript interface to the host class, which allows to access the host object from Javascript in the same way as from a C++ module.

To bind an interface to a Javascript object, call a creator in the generated JS wrapper object (in ifc/engine_interface.js.h):

```c++
    iref<js::ot::engine_interface> iface = js::ot::engine_interface::get(script_handle(url,true));
```

The script_handle class accepts urls or direct scripts. The url can be either a javascript file (*.js), or a html/htm file that should be opened and where the interface object resides.

Html files can be open with optional attributes encoded as query tokens (after ?, separated by &). The following attributes are recognized:

name            - window name
width           - initial window width
height          - initial window height
x, y            - initial window position
transparent     - 1/true, enable transparency
conceal         - 1/true, hide window from showing on screenshots

Since Javascript doesn’t support out or inout-type parameters, invocation of interface methods is a bit different from C++. In case there are multiple output-type parameters (including the return type if it’s not void), all out and inout values are returned encapsulated in a compound return value object as its members. The original return value is stored in $ret member in that case. If there is just one output value, the return value (or a single output parameter) is returned directly as the return value to Javascript.



### Creating interface objects from Javascript

Interface object can be created directly from the script by calling $query_interface injected Javascript function with the full interface name and creator function path as the first argument, followed by optional creator method arguments:

```Javascript
    //for the ot::engine_interface as defined above, with creator fn get(const char* param)

    var iobj = $query_interface(“ot::engine_interface.get”, “...”)
```

Sometimes it’s not desirable to expose all interface creator methods to Javascript, as they may be internal, requiring custom parameters that only make sense (or are available) on the C++ side. In that case, if the creator method begins with underscore character, it’s not visible to Javascript’s $query_interface call:

```c++
    class engine
    {
    public:
        ifc_class(ot::engine_interface, ”ifc/”)
    
        //a creator method not visible from Javascript
        ifc_fn static iref<engine> _get( const char* param );
    
        //a hidden creator can be renamed so that the underscore doesn’t appear in clients
        ifc_fnx(create) static iref<engine> _create();
    };
```


