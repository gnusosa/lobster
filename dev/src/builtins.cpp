// Copyright 2014 Wouter van Oortmerssen. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stdafx.h"

#include "vmdata.h"
#include "natreg.h"

#include "unicode.h"

using namespace lobster;

static RandomNumberGenerator<MersenneTwister> rnd;

int KeyCompare(const Value &a, const Value &b, bool rec = false)
{
    if (a.type != b.type)
        g_vm->BuiltinError("binary search: key type doesn't match type of vector elements");

    switch (a.type)
    {
        case V_INT:    return a.ival < b.ival ? -1 : a.ival > b.ival;
        case V_FLOAT:  return a.fval < b.fval ? -1 : a.fval > b.fval;
        case V_STRING: return strcmp(a.sval->str(), b.sval->str());

        case V_VECTOR:
            if (a.vval->len && b.vval->len && !rec) return KeyCompare(a.vval->at(0), b.vval->at(0), true);
            // fall thru:
        default:
            g_vm->BuiltinError("binary search: illegal key type");
            return 0;
    }
}

void AddBuiltins()
{
    STARTDECL(print) (Value &a)
    {
        Output(OUTPUT_PROGRAM, a.ToString(g_vm->programprintprefs).c_str());
        return a;
    }
    ENDDECL1(print, "x", "A", "A",
        "output any value to the console (with linefeed). returns its argument.");

    STARTDECL(set_print_depth) (Value &a) { g_vm->programprintprefs.depth = a.ival; return a; } 
    ENDDECL1(set_print_depth, "a", "I", "", 
        "for printing / string conversion: sets max vectors/objects recursion depth (default 10)");

    STARTDECL(set_print_length) (Value &a) { g_vm->programprintprefs.budget = a.ival; return a; } 
    ENDDECL1(set_print_length, "a", "I", "", 
        "for printing / string conversion: sets max string length (default 10000)");

    STARTDECL(set_print_quoted) (Value &a) { g_vm->programprintprefs.quoted = a.ival != 0; return a; } 
    ENDDECL1(set_print_quoted, "a", "I", "", 
        "for printing / string conversion: if the top level value is a string, whether to convert it with escape codes"
        " and quotes (default false)");

    STARTDECL(set_print_decimals) (Value &a) { g_vm->programprintprefs.decimals = a.ival; return a; } 
    ENDDECL1(set_print_decimals, "a", "I", "", 
        "for printing / string conversion: number of decimals for any floating point output (default -1, meaning all)");

    STARTDECL(getline) ()
    {
        const int MAXSIZE = 1000;
        char buf[MAXSIZE];
        fgets(buf, MAXSIZE, stdin);
        buf[MAXSIZE - 1] = 0;
        for (int i = 0; i < MAXSIZE; i++) if (buf[i] == '\n') { buf[i] = 0; break; }
        return Value(g_vm->NewString(buf, strlen(buf)));
    }
    ENDDECL0(getline, "", "", "S",
        "reads a string from the console if possible (followed by enter)");

    STARTDECL(if) (Value &c, Value &t, Value &e)
    {
        assert(0);  // Special case implementation in the VM
        (void)c;
        (void)t;
        (void)e;
        return Value();
    }
    ENDDECL3(if, "cond,then,else", "ACc", "A",
        "evaluates then or else depending on cond, else is optional");

    STARTDECL(while) (Value &c, Value &b)
    {
        assert(0);  // Special case implementation in the VM
        (void)c;
        (void)b;
        return Value();
    }
    ENDDECL2(while, "cond,body", "C@C", "A",
        "evaluates body while cond (converted to a function) holds true, returns last body value");

    STARTDECL(for) (Value &iter, Value &body)
    {
        assert(0);  // Special case implementation in the VM
        (void)iter;
        (void)body;
        return Value();
    }
    ENDDECL2(for, "iter,body", "AC", "I",
        "iterates over int/vector/string, body may take [ element [ , index ] ] arguments,"
        " returns number of evaluations that returned true");

    STARTDECL(append) (Value &v1, Value &v2)
    {
        auto nv = g_vm->NewVector(v1.vval->len + v2.vval->len, V_VECTOR);
        nv->append(v1.vval, 0, v1.vval->len); v1.DEC();
        nv->append(v2.vval, 0, v2.vval->len); v2.DEC();
        return Value(nv);
    }
    ENDDECL2(append, "xs,ys", "V*V1", "V1",
        "creates a new vector by appending all elements of 2 input vectors");

    STARTDECL(vector_reserve) (Value &len)
    {
        return Value(g_vm->NewVector(len.ival, V_VECTOR));
    }
    ENDDECL1(vector_reserve, "len", "I", "V*",
        "creates a new empty vector much like [] would, except now ensures"
        " it will have space for len push() operations without having to reallocate.");

    STARTDECL(length) (Value &a)
    {
        switch (a.type)
        {
            case V_INT:    return a;
            case V_VECTOR:
            case V_STRING: { auto len = a.lobj->len; a.DECRT(); return Value(len); }
            default: return g_vm->BuiltinError(string("illegal type passed to length: ") + BaseTypeName(a.type));
        }
    }
    ENDDECL1(length, "xs", "A", "I",
        "length of vector/string/int");

    STARTDECL(equal) (Value &a, Value &b)
    {
        bool eq = a.Equal(b, true);
        a.DEC();
        b.DEC();
        return Value(eq);
    }
    ENDDECL2(equal, "a,b", "AA", "I",
        "structural equality between any two values (recurses into vectors/objects,"
        " unlike == which is only true for vectors/objects if they are the same object)");

    STARTDECL(push) (Value &l, Value &x)
    {
        l.vval->push(x);
        return l;
    }
    ENDDECL2(push, "xs,x", "V*A1", "V1",
        "appends one element to a vector, returns existing vector");

    STARTDECL(pop) (Value &l)
    {
        if (!l.vval->len) { l.DEC(); g_vm->BuiltinError("pop: empty vector"); }
        auto v = l.vval->pop();
        l.DEC();
        return v;
    }
    ENDDECL1(pop, "xs", "V", "A1",
        "removes last element from vector and returns it");

    STARTDECL(top) (Value &l)
    {
        if (!l.vval->len) { l.DEC(); g_vm->BuiltinError("top: empty vector"); }
        auto v = l.vval->top();
        l.DEC();
        return v.INC();
    }
    ENDDECL1(top, "xs", "V", "A1",
        "returns last element from vector");

    STARTDECL(replace) (Value &l, Value &i, Value &a)
    {
        if (i.ival < 0 || i.ival >= l.vval->len) g_vm->BuiltinError("replace: index out of range");

        auto nv = g_vm->NewVector(l.vval->len, l.vval->type);
        nv->append(l.vval, 0, l.vval->len);
        l.DECRT();

        Value &dest = nv->at(i.ival);
        dest.DEC();
        dest = a;

        return Value(nv);
    }
    ENDDECL3(replace, "xs,i,x", "VIA1", "V1",
        "returns a copy of a vector with the element at i replaced by x");

    STARTDECL(insert) (Value &l, Value &i, Value &a, Value &n)
    {
        if (n.ival < 0 || i.ival < 0 || i.ival > l.vval->len)
            g_vm->BuiltinError("insert: index or n out of range");  // note: i==len is legal
        l.vval->insert(a, i.ival, max(n.ival, 1));
        return l;
    }
    ENDDECL4(insert, "xs,i,x,n", "VIAi", "V",
        "inserts n copies (default 1) of x into a vector at index i, existing elements shift upward,"
        " returns original vector");

    STARTDECL(remove) (Value &l, Value &i, Value &n)
    {
        int amount = max(n.ival, 1);
        if (n.ival < 0 || amount > l.vval->len || i.ival < 0 || i.ival > l.vval->len - amount)
            g_vm->BuiltinError("remove: index or n out of range");
        auto v = l.vval->remove(i.ival, amount);
        l.DEC();
        return v;
    }
    ENDDECL3(remove, "xs,i,n", "VIi", "A1",
        "remove element(s) at index i, following elements shift down. pass the number of elements to remove"
        " as an optional argument, default 1. returns the first element removed.");

    STARTDECL(removeobj) (Value &l, Value &o)
    {
        int removed = 0;
        for (int i = 0; i < l.vval->len; i++) if (l.vval->at(i).Equal(o, false))
        {
            l.vval->remove(i--, 1).DEC();
            removed++;
        }
        o.DEC();
        l.DEC();
        return Value(removed);
    }
    ENDDECL2(removeobj, "xs,obj", "VA", "I",
        "remove all elements equal to obj (==), returns amount of elements removed.");

    STARTDECL(binarysearch) (Value &l, Value &key)
    {
        ValueRef lref(l), kref(key);

        int size = l.vval->len;
        int i = 0;

        for (;;)
        {
            if (!size) break;

            int mid = size / 2;
            int comp = KeyCompare(key, l.vval->at(i + mid));

            if (comp)
            {
                if (comp < 0) size = mid;
                else { mid++; i += mid; size -= mid; }
            }
            else
            {
                i += mid;
                size = 1;
                while (i                      && !KeyCompare(key, l.vval->at(i - 1   ))) { i--; size++; }
                while (i + size < l.vval->len && !KeyCompare(key, l.vval->at(i + size))) {      size++; }
                break;
            }
        }

        g_vm->Push(Value(size));
        return Value(i);
    }
    ENDDECL2(binarysearch, "xs,key", "VA", "II",
        "does a binary search for key in a sorted vector, returns as first return value how many matches were found,"
        " and as second the index in the array where the matches start (so you can read them, overwrite them,"
        " or remove them), or if none found, where the key could be inserted such that the vector stays sorted."
        " As key you can use a int/float/string value, or if you use a vector, the first element of it will be used"
        " as the search key (allowing you to model a set/map/multiset/multimap using this one function). ");

    STARTDECL(copy) (Value &v)
    {
        auto nv = g_vm->NewVector(v.vval->len, v.vval->type);
        nv->append(v.vval, 0, v.vval->len);
        v.DECRT();
        return Value(nv);
    }
    ENDDECL1(copy, "xs", "V", "V1",
        "makes a shallow copy of vector/object.");

    STARTDECL(slice) (Value &l, Value &s, Value &e)
    {
        int size = e.ival;
        if (size < 0) size = l.vval->len + size;
        int start = s.ival;
        if (start < 0) start = l.vval->len + start;
        if (start < 0 || start + size > (int)l.vval->len)
            g_vm->BuiltinError("slice: values out of range");
        auto nv = g_vm->NewVector(size, V_VECTOR);
        nv->append(l.vval, start, size);
        l.DECRT();
        return Value(nv);
    }
    ENDDECL3(slice,
        "xs,start,size", "VII", "V1", "returns a sub-vector of size elements from index start."
        " start & size can be negative to indicate an offset from the vector length.");

    STARTDECL(any) (Value &v)
    {
        Value r(0, V_NIL);
        for (int i = 0; i < v.vval->len; i++)
        {
            if (v.vval->at(i).True())
            {
                r = v.vval->at(i);
                r.INC();
                break;
            }
        }
        v.DECRT();
        return r;
    }
    ENDDECL1(any, "xs", "V", "a1",
        "returns the first true element of the vector, or nil");

    STARTDECL(all) (Value &v)
    {
        Value r(true);
        for (int i = 0; i < v.vval->len; i++)
        {
            if (!v.vval->at(i).True())
            {
                r = Value(false);
                break;
            }
        }
        v.DECRT();
        return r;
    }
    ENDDECL1(all, "xs", "V", "I",
        "returns wether all elements of the vector are true values");

    STARTDECL(substring) (Value &l, Value &s, Value &e)
    {
        int size = e.ival;
        if (size < 0) size = l.vval->len + size;
        int start = s.ival;
        if (start < 0) start = l.vval->len + start;
        if (start < 0 || start + size > (int)l.vval->len)
            g_vm->BuiltinError("substring: values out of range");

        auto ns = g_vm->NewString(l.sval->str() + start, size);
        l.DECRT();
        return Value(ns);
    }
    ENDDECL3(substring, "s,start,size", "SII", "S", 
        "returns a substring of size characters from index start."
        " start & size can be negative to indicate an offset from the string length.");

    STARTDECL(tokenize) (Value &s, Value &delims, Value &whitespace)
    {
        auto v = g_vm->NewVector(0, V_VECTOR);
        auto ws = whitespace.sval->str();
        auto dl = delims.sval->str();
        auto p = s.sval->str();
        p += strspn(p, ws);
        auto strspn1 = [](char c, const char *set) { while (*set) if (*set == c) return 1; return 0; };
        while (*p)
        {
            auto delim = p + strcspn(p, dl);
            auto end = delim;
            while (end > p && strspn1(end[-1], ws)) end--;
            v->push(g_vm->NewString(p, end - p));
            p = delim + strspn(delim, dl);
            p += strspn(p, ws);
        }
        s.DECRT();
        delims.DECRT();
        whitespace.DECRT();
        return Value(v);
    }
    ENDDECL3(tokenize, "s,delimiters,whitespace", "SSS", "S]",
        "splits a string into a vector of strings, by splitting into segments upon each dividing or terminating"
        " delimiter. Segments are stripped of leading and trailing whitespace."
        " Example: \"; A ; B C; \" becomes [ \"\", \"A\", \"B C\" ] with \";\" as delimiter and \" \" as whitespace." );

    STARTDECL(unicode2string) (Value &v)
    {
        ValueRef vref(v);
        char buf[7];
        string s;
        for (int i = 0; i < v.vval->len; i++)
        {
            auto &c = v.vval->at(i);
            if (c.type != V_INT) g_vm->BuiltinError("unicode2string: vector contains non-int values.");
            ToUTF8(c.ival, buf);
            s += buf;
        }
        return Value(g_vm->NewString(s));
    }
    ENDDECL1(unicode2string, "us", "I]", "S",
        "converts a vector of ints representing unicode values to a UTF-8 string.");

    STARTDECL(string2unicode) (Value &s)
    {
        ValueRef sref(s);
        auto v = g_vm->NewVector(s.sval->len, V_VECTOR);
        ValueRef vref((Value(v)));
        const char *p = s.sval->str();
        while (*p)
        {
            int u = FromUTF8(p);
            if (u < 0) return Value(0, V_NIL);
            v->push(u);
        }
        return Value(v).INC();
    }
    ENDDECL1(string2unicode, "s", "S", "I]?",
        "converts a UTF-8 string into a vector of unicode values, or nil upon a decoding error");

    STARTDECL(number2string) (Value &n, Value &b, Value &mc)
    {
        if (b.ival < 2 || b.ival > 36 || mc.ival > 32)
            g_vm->BuiltinError("number2string: values out of range");

        uint i = (uint)n.ival;
        string s;
        const char *from = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        while (i || (int)s.length() < mc.ival)
        {
            s.insert(0, 1, from[i % b.ival]);
            i /= b.ival;
        }

        return Value(g_vm->NewString(s));
    }
    ENDDECL3(number2string, "number,base,minchars", "III", "S",
        "converts the (unsigned version) of the input integer number to a string given the base (2..36, e.g. 16 for"
        " hex) and outputting a minimum of characters (padding with 0).");

    STARTDECL(pow) (Value &a, Value &b) { return Value(powf(a.fval, b.fval)); } ENDDECL2(pow, "a,b", "FF", "F",
        "a raised to the power of b");

    STARTDECL(log) (Value &a) { return Value(logf(a.fval)); } ENDDECL1(log, "a", "F", "F", 
        "natural logaritm of a");

    STARTDECL(sqrt) (Value &a) { return Value(sqrtf(a.fval)); } ENDDECL1(sqrt, "f", "F", "F", 
        "square root");

    STARTDECL(and) (Value &a, Value &b) { return Value(a.ival & b.ival);  } ENDDECL2(and, "a,b", "II", "I",
        "bitwise and");
    STARTDECL(or)  (Value &a, Value &b) { return Value(a.ival | b.ival);  } ENDDECL2(or,  "a,b", "II", "I", 
        "bitwise or");
    STARTDECL(xor) (Value &a, Value &b) { return Value(a.ival ^ b.ival);  } ENDDECL2(xor, "a,b", "II", "I",
        "bitwise exclusive or");
    STARTDECL(not) (Value &a)           { return Value(~a.ival);          } ENDDECL1(not, "a",   "I",  "I",
        "bitwise negation");
    STARTDECL(shl) (Value &a, Value &b) { return Value(a.ival << b.ival); } ENDDECL2(shl, "a,b", "II", "I", 
        "bitwise shift left");
    STARTDECL(shr) (Value &a, Value &b) { return Value(a.ival >> b.ival); } ENDDECL2(shr, "a,b", "II", "I", 
        "bitwise shift right");

    #define VECTOROP(name, op, otype) \
        if (a.type == V_VECTOR) { \
            auto v = g_vm->NewVector(a.vval->len, a.vval->type); \
            for (int i = 0; i < a.vval->len; i++) { \
                auto f = a.vval->at(i); \
                if (otype == V_FLOAT && f.type == V_INT) f = Value(float(f.ival)); \
                if (f.type != otype) { a.DECRT(); v->deleteself(); goto err; } \
                v->push(Value(op)); \
            } \
            a.DECRT(); \
            return Value(v); \
        } \
        err: g_vm->BuiltinError(#name " requires numeric vector argument"); \
        return Value();
    #define VECTOROPF(name, op) VECTOROP(name, op, V_FLOAT)
    #define VECTOROPI(name, op) VECTOROP(name, op, V_INT)

    STARTDECL(ceiling) (Value &a) { return Value(int(ceilf(a.fval))); } ENDDECL1(ceiling, "f", "F", "I",
        "the nearest int >= f");
    STARTDECL(ceiling) (Value &a) { VECTOROPF(ceiling, int(ceilf(f.fval))); } ENDDECL1(ceiling, "v", "F]", "I]:/",
        "the nearest ints >= each component of v");

    STARTDECL(floor)   (Value &a) { return Value(int(floorf(a.fval))); } ENDDECL1(floor, "f", "F", "I",
        "the nearest int <= f");
    STARTDECL(floor)   (Value &a) { VECTOROPF(floor, int(floorf(f.fval))); } ENDDECL1(floor, "v", "F]", "I]:/",
        "the nearest ints <= each component of v");

    STARTDECL(int)(Value &a) { return Value(int(a.fval)); } ENDDECL1(int, "f", "F", "I",
        "converts a float to an int by dropping the fraction");
    STARTDECL(int)(Value &a) { VECTOROPF(int, int(f.fval)); } ENDDECL1(int, "v", "F]", "I]:/",
        "converts a vector of floats to ints by dropping the fraction");

    STARTDECL(round)   (Value &a) { return Value(int(a.fval + 0.5f)); } ENDDECL1(round, "f", "F", "I",
        "converts a float to the closest int");
    STARTDECL(round)   (Value &a) { VECTOROPF(round, int(f.fval + 0.5f)); } ENDDECL1(round, "v", "F]", "I]:/",
        "converts a vector of floats to the closest ints");

    STARTDECL(fraction)(Value &a) { return Value(a.fval - floorf(a.fval)); } ENDDECL1(fraction, "f", "F", "F",
        "returns the fractional part of a float: short for f - floor(f)");
    STARTDECL(fraction)(Value &a) { VECTOROPF(fraction, f.fval - floorf(f.fval)); } ENDDECL1(fraction, "v", "F]", "F]:/",
        "returns the fractional part of a vector of floats");

    STARTDECL(float)(Value &a) { return Value(float(a.ival)); } ENDDECL1(float, "i", "I", "F",
        "converts an int to float");
    STARTDECL(float)(Value &a) { VECTOROPI(float, float(f.ival)); } ENDDECL1(float, "v", "I]", "F]:/",
        "converts a vector of ints to floats");

    STARTDECL(sin) (Value &a) { return Value(sinf(a.fval * RAD)); } ENDDECL1(sin, "angle", "F", "F",
        "the y coordinate of the normalized vector indicated by angle (in degrees)");
    STARTDECL(cos) (Value &a) { return Value(cosf(a.fval * RAD)); } ENDDECL1(cos, "angle", "F", "F",
        "the x coordinate of the normalized vector indicated by angle (in degrees)");

    STARTDECL(sincos) (Value &a) { return ToValue(float3(cosf(a.fval * RAD), sinf(a.fval * RAD), 0.0f)); }
    ENDDECL1(sincos, "angle", "F", "F]:3",
        "the normalized vector indicated by angle (in degrees), same as [ cos(angle), sin(angle), 0 ]");

    STARTDECL(arcsin) (Value &y) { return Value(asinf(y.fval) / RAD); } ENDDECL1(arcsin, "y", "F", "F",
        "the angle (in degrees) indicated by the y coordinate projected to the unit circle");
    STARTDECL(arccos) (Value &x) { return Value(acosf(x.fval) / RAD); } ENDDECL1(arccos, "x", "F", "F",
        "the angle (in degrees) indicated by the x coordinate projected to the unit circle");

    STARTDECL(atan2) (Value &vec) { auto v = ValueDecTo<float3>(vec); return Value(atan2f(v.y(), v.x()) / RAD); } 
    ENDDECL1(atan2, "vec",  "F]" , "F",
        "the angle (in degrees) corresponding to a normalized 2D vector");

    STARTDECL(normalize) (Value &vec)
    {
        switch (vec.vval->len)
        {
            case 2: { auto v = ValueDecTo<float2>(vec); return ToValue(v == float2_0 ? v : normalize(v)); }
            case 3: { auto v = ValueDecTo<float3>(vec); return ToValue(v == float3_0 ? v : normalize(v)); }
            case 4: { auto v = ValueDecTo<float4>(vec); return ToValue(v == float4_0 ? v : normalize(v)); }
            default: return g_vm->BuiltinError("normalize() only works on vectors of length 2 to 4");
        }
    }
    ENDDECL1(normalize, "vec",  "F]" , "F]:/",
        "returns a vector of unit length");

    STARTDECL(dot) (Value &a, Value &b) { return Value(dot(ValueDecTo<float4>(a), ValueDecTo<float4>(b))); }
    ENDDECL2(dot,   "a,b", "F]F]", "F",
        "the length of vector a when projected onto b (or vice versa)");

    STARTDECL(magnitude) (Value &a)  { return Value(length(ValueDecTo<float4>(a))); } ENDDECL1(magnitude, "a", "A]", "F",
        "the geometric length of a vector");

    STARTDECL(cross) (Value &a, Value &b) { return ToValue(cross(ValueDecTo<float3>(a), ValueDecTo<float3>(b))); }
    ENDDECL2(cross, "a,b", "F]F]", "F]:3",
        "a perpendicular vector to the 2D plane defined by a and b (swap a and b for its inverse)");

    STARTDECL(rnd) (Value &a) { return Value(rnd(max(1, a.ival))); } ENDDECL1(rnd, "max", "I", "I",
        "a random value [0..max).");
    STARTDECL(rnd) (Value &a) { VECTOROPI(rnd, rnd(max(1, f.ival))); } ENDDECL1(rnd, "max", "I]", "I]:/",
        "a random vector within the range of an input vector.");
    STARTDECL(rndfloat)() { return Value((float)rnd.rnddouble()); } ENDDECL0(rndfloat, "", "", "F",
        "a random float [0..1)");
    STARTDECL(rndseed) (Value &seed) { rnd.seed(seed.ival); return Value(); } ENDDECL1(rndseed, "seed", "I", "",
        "explicitly set a random seed for reproducable randomness");

    STARTDECL(div) (Value &a, Value &b) { return Value(float(a.ival) / float(b.ival)); } ENDDECL2(div, "a,b", "II", "F",
        "forces two ints to be divided as floats");

    STARTDECL(clamp) (Value &a, Value &b, Value &c)
    {
        return Value(max(min(a.ival, c.ival), b.ival));
    }
    ENDDECL3(clamp, "x,min,max", "III", "I",
        "forces an integer to be in the range between min and max (inclusive)");

    STARTDECL(clamp) (Value &a, Value &b, Value &c)
    {
        return Value(max(min(a.fval, c.fval), b.fval));
    }
    ENDDECL3(clamp, "x,min,max", "FFF", "F",
             "forces a float to be in the range between min and max (inclusive)");

    STARTDECL(abs) (Value &a)
    {
        switch (a.type)
        {
            case V_INT:    return Value(a.ival >= 0 ? a.ival : -a.ival);
            case V_FLOAT:  return Value(a.fval >= 0 ? a.fval : -a.fval);
            case V_VECTOR: {
                auto v = g_vm->NewVector(a.vval->len, a.vval->type);
                for (int i = 0; i < a.vval->len; i++)
                {
                    auto f = a.vval->at(i);
                    switch (f.type)
                    {
                        case V_INT: v->push(Value(abs(f.ival))); break;
                        case V_FLOAT: v->push(Value(fabsf(f.fval))); break;
                        default: v->deleteself(); goto err;
                    }
                }
                a.DECRT();
                return Value(v);
            }
            default: break;
        }
        err:
        a.DECRT();
        return g_vm->BuiltinError("abs() needs a numerical value or numerical vector");
    }
    ENDDECL1(abs, "x", "A", "A1",
        "absolute value of int/float/vector");

    #define MINMAX(op,name) \
        switch (x.type) \
        { \
            case V_INT: \
                if (y.type == V_INT) return Value(x.ival op y.ival ? x.ival : y.ival); \
                else if (y.type == V_FLOAT) return Value(x.ival op y.fval ? x.ival : y.fval); \
                break; \
            case V_FLOAT: \
                if (y.type == V_INT) return Value(x.fval op y.ival ? x.fval : y.ival); \
                else if (y.type == V_FLOAT) return Value(x.fval op y.fval ? x.fval : y.fval); \
                break; \
            case V_VECTOR: \
                return ToValue(name(ValueDecTo<float4>(x), ValueDecTo<float4>(y))); \
            default: ; \
        } \
        return g_vm->BuiltinError("illegal arguments to min/max");

    STARTDECL(min) (Value &x, Value &y) { MINMAX(<,min) } ENDDECL2(min, "x,y", "A*A1", "A1",
        "smallest of 2 int/float values. Also works on vectors of int/float up to 4 components, returns a vector of float.");
    STARTDECL(max) (Value &x, Value &y) { MINMAX(>,max) } ENDDECL2(max, "x,y", "A*A1", "A1",
        "largest of 2 int/float values. Also works on vectors of int/float up to 4 components, returns a vector of float.");

    #undef MINMAX

    STARTDECL(cardinalspline) (Value &z, Value &a, Value &b, Value &c, Value &f, Value &t)
    {
        return ToValue(cardinalspline(ValueDecTo<float3>(z),
                                       ValueDecTo<float3>(a),
                                       ValueDecTo<float3>(b),
                                       ValueDecTo<float3>(c), f.fval, t.fval));
    }
    ENDDECL6(cardinalspline, "z,a,b,c,f,tension", "F]F]F]F]FF", "F]:3",
        "computes the position between a and b with factor f [0..1], using z (before a) and c (after b) to form a"
        " cardinal spline (tension at 0.5 is a good default)");

    STARTDECL(lerp) (Value &x, Value &y, Value &f)
    {
        if (x.type == y.type)
        {
            switch (x.type)
            {
                case V_FLOAT:  return Value(mix(x.fval, y.fval, f.fval));
                case V_INT:    return Value(mix((float)x.ival, (float)y.ival, f.fval));
                               // should this do any size vecs?
                case V_VECTOR: return ToValue(mix(ValueDecTo<float4>(x), ValueDecTo<float4>(y), f.fval));
                default: ;
            }
        }
        g_vm->BuiltinError("illegal arguments passed to lerp()");
        return Value();
    }
    ENDDECL3(lerp, "x,y,f", "AAF", "A1",
        "linearly interpolates between x and y (float/int/vector) with factor f [0..1]");


    STARTDECL(resume) (Value &co, Value &ret)
    {
        g_vm->CoResume(co.cval);
        return ret;
    }
    ENDDECL2(resume, "coroutine,returnvalue", "Ra", "A",
        "resumes execution of a coroutine, passing a value back or nil");

    STARTDECL(returnvalue) (Value &co)
    {
        Value &rv = co.cval->Current().INC();
        co.DECRT();
        return rv;
    }
    ENDDECL1(returnvalue, "coroutine", "R", "A",
        "gets the last return value of a coroutine");

    STARTDECL(active) (Value &co)
    {
        bool active = co.cval->active;
        co.DECRT();
        return Value(active);
    }
    ENDDECL1(active, "coroutine", "R", "I",
        "wether the given coroutine is still active");

    STARTDECL(program_name) ()
    {
        return Value(g_vm->NewString(g_vm->GetProgramName()));
    }
    ENDDECL0(program_name, "", "", "S",
        "returns the name of the main program (e.g. \"foo.lobster\".");

    STARTDECL(caller_id) ()
    {
        return Value(g_vm->CallerId());
    }
    ENDDECL0(caller_id, "", "", "I",
        "returns an int that uniquely identifies the caller to the current function.");

    STARTDECL(seconds_elapsed) ()
    {
        return Value(g_vm->Time());
    }
    ENDDECL0(seconds_elapsed, "", "", "F",
        "seconds since program start as a float, unlike gl_time() it is calculated every time it is called");

    STARTDECL(assert) (Value &c)
    {
        if (!c.True()) g_vm->BuiltinError("assertion failed");
        c.DEC();
        return Value();
    }
    ENDDECL1(assert, "condition", "A", "",
        "halts the program with an assertion failure if passed false");

    STARTDECL(trace_bytecode) (Value &i)
    {
        g_vm->Trace(i.ival != 0);
        return Value();
    }
    ENDDECL1(trace_bytecode, "on", "I", "",
        "tracing shows each bytecode instruction as it is being executed, not very useful unless you are trying to"
        " isolate a compiler bug");

    STARTDECL(collect_garbage) ()
    {
        return Value(g_vm->GC());
    }
    ENDDECL0(collect_garbage, "", "", "I",
        "forces a garbage collection to re-claim cycles. slow and not recommended to be used. instead, write code"
        " to clear any back pointers before abandoning data structures. Watch for a \"LEAKS FOUND\" message in the"
        " console upon program exit to know when you've created a cycle. returns amount of objects collected.");

    STARTDECL(set_max_stack_size) (Value &max)
    {
        g_vm->SetMaxStack(max.ival * 1024 * 1024 / sizeof(Value));
        return max;
    }
    ENDDECL1(set_max_stack_size, "max",  "I", "",
        "size in megabytes the stack can grow to before an overflow error occurs. defaults to 1");
}

AutoRegister __abi("builtins", AddBuiltins);

