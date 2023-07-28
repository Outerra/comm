# Metagen

Metagen is a text/code generator, that can be used to generate complex documents - html reports, source files etc - directly from C++ objects, using the provided document template and an instance of a class that has metastream operator defined.
Metagen can generate repeating blocks of text with tags that bind to an array-type variables; it can contain conditions that drive the generation, etc.

## Simple value substitution

Let's have a pattern file containing the following text:
```
My name is $name$.
```

When invoking the code generator with this file and with a simple variable `name` containing value `John`, it generates the expected output:
```
My name is John.
```

A simple identifier between two $ characters is a variable name, whose value will be substituted there. An empty string is inserted if such variable doesn't exist, unless the default attribute was provided.
The string associated with the default attribute is used only when the variable doesn't exist, not when it contains an empty string.
```
My name is $name default="Frank"$.
```
Attributes such as `default` are used to refine the behavior of metagen.

## Compound variables

Variables provided to metagen can be simple primitive types, but also complex structures or arrays. If the requested value is a property of a compound object, it can be accessed using the . dot operator after the compound object name.
Multiple chained dot operators can be used to get down to the needed property.
```
My name is $person.name$.
Born on $person.birth.month$ $person.birth.day$, $person.birth.year$.
```

To avoid the need to repeat the same part of the variable name, part of the input file can be wrapped within structure tags, that cause every variable access to be relative to the path specified in the tag.
Structure opening and closing tags are written as `${name}$` and `${/name}$`, respectively. The above line can be then rewritten to:
```
Born on ${person.birth}$$month$ $day$, $year$${/person.birth}$.
```

Structure tags must be nested properly, it's not really required for the closing structure tag to specify the path again, except for verification purposes. Otherwise it can be reduced to a simple `${/}$` tag.


## Accessing outer scope variables

If the variable name in a simple tag or within structure or array tags starts with a dot (.), it signifies accessing variables from outside of the nearest structure (or array) tag.
So for example, from inside the `${person.birth}$` tags, specifying `$.name$` would refer to a variable from the scope of the person variable, i.e. it would refer to `person.name` variable.
```
${person.birth}$$.name$ was born on $month$ $day$, $year$${/}$.
```

It's possible to put more than one dot in front of a name, each one emerging up from the nearest structure/array scope to its parent.


## Generating sequences from arrays

In addition to compound objects and simple variables, input object can also contain arrays (or any iteratable containers). A variable containing an array can be bound to a special array-tag, that is invoked for each array element subsequently. Example, given the template:
```
class $classname$ {
$[param]$
    $type$ $name$;
$[/param]$
};
```
and the argument being an instance of C++ class containing (here streamed in cxx format):
```
classname = "Foo"
param = [{ type = "int", name = "i" } { type = "float", name = "f" }]
```

the generator outputs the following text:
```
class Foo {
    int i;
    float f;
};
```
Fragments enclosed between `$[name]$` and `$[/name]$` tags should be bound to an array-type variable, and are subsequently invoked for each array element.
Substitutions within this fragment do address variables defined in the currently accessed array element. To reach a variable from the outer scope, the name should be preceded with one or more dot (.) characters, as described in the section "Accessing outer scope variables".

The fragment may contain additional attributes that define the generator behavior in certain cases.
For example, it's possible to specify chunks of text to be inserted before the first and each of the rest elements (used for placing separators between the elements when needed).
Or to define chunks of text to be inserted after the last element but only when the array is non-empty.
This info can be provided either in tag-format or in attribute-format. The difference is that in the tag format the text is interpreted literally (except the nested `$...$` tags), with all newlines respected etc, while in the attribute format the text is processed and the escape sequences are translated.

It means it's possible to write either
```
$[param rest]$
$[param]$
...
$[/param]$
```
or
```
$[param rest="\n"]$
...
$[/param]$
```
Note that the first form inserts a newline before each element after the first one, since a newline is a part of the inner code.

Allowed tags are specified in the following table:

|as a tag | as an attribute | function |
|---------|-----------------|----------|
| `$[name first]$` | `$[name first="..."]$` | text inserted before the first element |
| `$[name rest]$` | `$[name rest="..."]$` | text inserted before all elements except the first one |
| `$[name after]$` | `$[name after="..."]$` | text inserted after the last element, if there was at least one element evaluated (see below) |
| _text between array tags_ | `$[name body="..."]$` | text used for each element |
| `$[name empty]$` | `$[name empty="..."]$` | text used only if the array was empty |

The array tag can contain additional conditional attributes, that are applied element-wise to determine whether the particular array element should be filtered out.
For example, given the following tag, its content will be evaluated for each element of the args array, but only if the element contains a variable with a value that evaluates to true (with `?` conditions) or to false (with `!` conditions):
```
$[args rest=" ," ?filled]$ .... $[/args]$
$[args rest=" ," !set]$ .... $[/args]$
```
See more details in the "Conditions" section.

Note that the `after` tag is evaluated only when at least one element passed the conditions and has been evaluated.
To generate a simple list of classes and their methods, one can write the following template:
```
$[class rest]$

//////////////////////////////////////////////////////////////////////////////
$[class]$
class $name$
{
public:
$[method]$
    void $name$($[param rest="," after=" "]$ $type$ $name$$[/param]$);
$[/method]$
};
$[/class]$
```

The code generator is given the template along with bindings, that can be set either programmatically or from a structured file (xml or cxx or any metastream formatted type file would work):
```
class = [
    {name = "Foo",
    method = [
    {name = "foo", param = [{type="int", name="i"}, {type="float", name="f"}]}
    {name = "bar", param = [{type="const char*", name="s"}]}
    ]}

    {name = "Bar",
    method = [
    {name = "foo", param = [{type="void*", name="p"}, {type="uint", name="n"}]}
    ]}
]
```

And the output will be:
```
class Foo
{
    void foo( int i, float f );
    void bar( const char* s );
};

//////////////////////////////////////////////////////////////////////////////
class Bar
{
    void foo( void* p, uint n );
};
```

### Special array variables

Special variables that can be used within array scope:
| variable | meaning |
|----------|---------|
| `$@index$` | return current element index (0 based) in array |
| `$@order$` | return the position of the current element in a filtered array (see array tags with conditions) |
| `$@value$` | return array element value (arrays of primitive types) |
| `$@size$`<br>`$@size >cond$` | return the array size<br>return the filtered array size |

For simple processing of arrays of strings it's also possible to use the simple tag.
Let's say stuff is an array of strings, then $stuff$ will simply output a string concatenated from the individual elements. Additionally, the first/rest/after attributes can be used with the simple tag as well:

| attribute | example | description |
|-----------|---------|-------------|
|           | `$stuff$` | a tightly concatenated string made of array elements |
| first     | `$stuff first="-"$` | prefix the first element by given string (only for non-empty arrays) |
| rest      | `$stuff rest="."$` | infix separator, a string to put before all elements but the first one |
| after     | `$stuff after=";"$` | suffix after the last element, if the array is not empty |
| empty     | `$stuff empty="nullptr"` | text generated when the array is empty |

The attributes can be freely combined.

To get the tag to generate escaped text, use` $stuff\"$` or `$stuff\'$`. The escape sign should appear after all optional attributes. 


## Conditions

Tags formatted as `$(if cond)$ ... $(/if)$` allow to conditionally insert a chunk of text depending on the evaluated condition.
If a simple variable name is provided, the tag content is processed if the variable is defined and contains value that can be interpreted as true.
With the use of aditional tag attributes it's possible to condition the content processing depending on whether the variable is (non)empty, is (not) an array etc.
Conditional attributes are prefixed either with the `?` character or with the `!` character.

| conditional tag | description (do if) |
|-----------------|---------------------|
| `$(if ?var)$` | variable is a non-empty array (also a string), compound or non-zero number |
| `$(if !var)$` | variable is an empty array, empty string, or number zero |
| `$(elif)$` | always true, used as unconditional else clause |
| `$(elif ?cond)$` | used as an else if clause, processed only after previous blocks didn't met the condition |
| `$(/if)$` | end of the conditional block |
| `$(if ?var.defined)$` | variable is defined |
| `$(if ?var.array)$` | variable is an array |
| `$(if ?var.nonzero)$`<br>`$(if ?var.true)$`<br>`$(if ?var)$` | variable is a non-empty array (also a string), compound or non-zero number |
| `$(if ?var.empty)$`<br>`$(if ?var.false?)$`<br>`$(if !var)$` | if var is an array, returns true if empty |
| `$(if ?var="string")$` | variable converted to string contains the specified string |
| `$(if !var="string")$` | variable converted to string does not contain the specified string |
| `$(if ?var1 ?var2)$`<br>`$(if & ?var1 ?var2)$` | both conditions pass |
| `$(if \| ?var1 ?var2)$` | either condition passes |
| `$(if somearray ?prop)$` | variable/array has members/elements that test true by condition(s) that follow |
| `$(if somearray ?var)$`<br>`$(if somearray ?var1 ?var2)$`<br>`$(if somearray \| ?var1 ?var2)$` | if array has at least one element that satisfies the condition<br>also works on non-array types |

Example:
```
$(if !client.defined)$
#include "server.h"
$(elif ?client.empty)$
#include "generic_client.h"
$(elif)$
#include "$client$$(if !client.ends=".h")$.h$(/if)$"
$(/if)$
```

If the conditional variable is an array, normally the if clause passes if the array is non-empty. It's also possible to query if array with elements filtered by a condition would be non-zero too:
```
$(if somearray ?prop)$

### Multiple conditions

Multiple conditions in an `if` tag are by default treated as tied by logical and, i.e. all conditions must be met for the condition to pass.
```
$(if ?type="int" ?value.nonzero)$
```
To specify the logical operator to use, insert `|` or `&` after `if`:
```
$(if | ?value="1" ?value="2")$ ... $(/if)$
$(if & ?type="int" ?value.nonzero)$ ... $(/if)$
```

### Conditions in array evaluation

Conditions can ba also used in the array tag to filter array elements. Only the array elements that pass the condition (i.e. those that have `param` property non-zero) are used:
```
$[somearray ?param]$ $value$ $[/somearray]$
```

Array tag properties like `first`, `rest`, `empty` apply on the filtered content. Special value `@index` still refers to the original (unfiltered) element position, whereas `@order` indexes the filtered one.


## Removing newlines

Metagen outputs verbatim all static text found outside tags. This means that any newlines before/after tags are written to the output too, even if the newline was intended just for clarity of the document template, and it was not desired to appear in the output.
If a tag contains a '-' character just after the leading `$` character or just before the trailing one, it is interpreted as a request to remove one newline in front or behind the tag, respectively, alongwith any additional white spaces characters in between.

In the following example are the hyphens added to remove extra newlines after the [class] and [method] tags.
```
//////////////////////////////////////////////////////////////////////////////
$[class]-$
class $name$
{
public:
$[method]-$
    void $name$($[param rest="," after=" "]$ $type$ $name$$[/param]$);
$[/method]$
};
$[/class]$
```
