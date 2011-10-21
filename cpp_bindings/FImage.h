#ifndef FIMAGE_H
#define FIMAGE_H

#include "MLVal.h"
#include <vector>
#include <stdint.h>
#include <string>
#include <sstream>

namespace FImage {

    class Image;
    class Var;
    class Func;

    // objects with unique auto-generated names
    template<char c>
    class Named {
    public:        
        Named() {
            std::ostringstream ss;
            ss << c;
            ss << _instances++;
            _name = ss.str();
        }

        Named(const std::string &name) : _name(name) {}

        const std::string &name() const {return _name;}

    private:
        static int _instances;
        std::string _name;
    };

    // A node in an expression tree.
    class Expr {
    public:
        Expr();
        Expr(MLVal);
        Expr(int32_t);
        Expr(unsigned);
        Expr(float);

        void operator+=(const Expr &);
        void operator-=(const Expr &);
        void operator*=(const Expr &);
        void operator/=(const Expr &);
        
        MLVal node;
        void debug();

        // The list of argument buffers contained within subexpressions
        std::vector<Image *> bufs;

        // The list of free variables found
        std::vector<Var *> vars;

        // The list of functions directly called
        std::vector<Func *> funcs;

        // declare that this node has a child for bookkeeping
        void child(const Expr &c);
    };

    Expr operator+(const Expr &, const Expr &);
    Expr operator-(const Expr &, const Expr &);
    Expr operator*(const Expr &, const Expr &);
    Expr operator/(const Expr &, const Expr &);
    
    Expr select(const Expr &, const Expr &, const Expr &);
    Expr operator>(const Expr &, const Expr &);
    Expr operator>=(const Expr &, const Expr &);
    Expr operator>(const Expr &, const Expr &);
    Expr operator<=(const Expr &, const Expr &);
    Expr operator!=(const Expr &, const Expr &);
    Expr operator==(const Expr &, const Expr &);

    // Make a debug node
    Expr Debug(Expr, const std::string &prefix, const std::vector<Expr> &args);
    Expr Debug(Expr, const std::string &prefix);
    Expr Debug(Expr, const std::string &prefix, Expr a);
    Expr Debug(Expr, const std::string &prefix, Expr a, Expr b);
    Expr Debug(Expr, const std::string &prefix, Expr a, Expr b, Expr c);
    Expr Debug(Expr, const std::string &prefix, Expr a, Expr b, Expr c, Expr d);
    Expr Debug(Expr, const std::string &prefix, Expr a, Expr b, Expr c, Expr d, Expr e);

    // A loop variable with the given (static) range [min, max)
    class Var : public Expr, public Named<'v'> {
    public:
        Var();
        Var(const std::string &name);
    };

    class Range {
    public:
        Range() {}
        Range(Expr min, Expr size) : range {std::pair<Expr, Expr> {min, size}} {}
        std::vector<std::pair<Expr, Expr> > range;
    };

    Range operator*(const Range &a, const Range &b);

    // A function call (if you cast it to an expr), or a function definition lhs (if you assign an expr to it).
    class FuncRef {
    public:
        // Yay C++0x initializer lists
        FuncRef(Func *f, const Expr &a) : 
            f(f), func_args{a} {}
        FuncRef(Func *f, const Expr &a, const Expr &b) :
            f(f), func_args{a, b} {}
        FuncRef(Func *f, const Expr &a, const Expr &b, const Expr &c) :
            f(f), func_args{a, b, c} {}
        FuncRef(Func *f, const Expr &a, const Expr &b, const Expr &c, const Expr &d) : 
            f(f), func_args{a, b, c, d} {}
        FuncRef(Func *f, const std::vector<Expr> &args) :
            f(f), func_args(args) {}

        // Turn it into a function call
        operator Expr();

        // This assignment corresponds to definition. This FuncRef is
        // defined to have the given expression as its value.
        void operator=(const Expr &e);
        
        // Make sure we don't directly assign an FuncRef to an FuncRef (but instead treat it as a definition)
        void operator=(const FuncRef &other) {*this = (const Expr &)other;}
                        
        // A pointer to the function object that this lhs defines.
        Func *f;

        std::vector<Expr> func_args;

    };

    class Func : public Named<'f'> {
    public:
        Func() : function_ptr(NULL) {}
        Func(const std::string &name) : Named<'f'>(name), function_ptr(NULL) {}

        // Define a function
        void define(const std::vector<Expr> &func_args, const Expr &rhs);
        
        // Generate a call to the function (or the lhs of a definition)
        FuncRef operator()(const Expr &a) {return FuncRef(this, a);}
        FuncRef operator()(const Expr &a, const Expr &b) {return FuncRef(this, a, b);}
        FuncRef operator()(const Expr &a, const Expr &b, const Expr &c) {return FuncRef(this, a, b, c);}     
        FuncRef operator()(const Expr &a, const Expr &b, const Expr &c, const Expr &d) {return FuncRef(this, a, b, c, d);}  
        FuncRef operator()(const std::vector<Expr> &args) {return FuncRef(this, args);}

        // Print every time this function gets evaluated
        void trace();
        
        // Generate an image from this function by Jitting the IR and running it.
        Image realize(int);
        Image realize(int, int);
        Image realize(int, int, int);
        Image realize(int, int, int, int);
        void realize(Image);
        
        /* These methods generate a partially applied function that
         * takes a schedule and modifies it. These functions get pushed
         * onto the schedule_transforms vector, which is traversed in
         * order starting from an initial default schedule to create a
         * mutated schedule */
        void split(const Var &, const Var &, const Var &, int factor);
        void vectorize(const Var &);
        void unroll(const Var &);
        void transpose(const Var &, const Var &);
        void chunk(const Var &, const Range &);

        /* The space of all living functions (TODO: remove a function
           from the environment when it goes out of scope) */
        static MLVal *environment;

        // The scalar value returned by the function
        Expr rhs;
        std::vector<Expr> args;
        MLVal arglist;

    protected:

        /* The ML definition object (name, return type, argnames, body)
           The body here evaluates the function over an entire range,
           and the arg list will include a min and max value for every
           free variable. */
        MLVal definition;

        /* A list of schedule transforms to apply when realizing. These should be
           partially applied ML functions that map a schedule to a schedule. */
        std::vector<MLVal> schedule_transforms;

        // The compiled form of this function
        mutable void (*function_ptr)(void *); 
    };

    // The image type. Has from 1 to 4 dimensions.
    class Image : public Named<'i'> {
    public:
        Image(uint32_t);
        Image(uint32_t, uint32_t);
        Image(uint32_t, uint32_t, uint32_t);
        Image(uint32_t, uint32_t, uint32_t, uint32_t);
        
        ~Image();
        
        // make a Load
        Expr operator()(const Expr &);
        Expr operator()(const Expr &, const Expr &);
        Expr operator()(const Expr &, const Expr &, const Expr &);
        Expr operator()(const Expr &, const Expr &, const Expr &, const Expr &);
        
        // Actually look something up in the image. Won't return anything
        // interesting if the image hasn't been evaluated yet.
        float &operator()(int a) {
            return data[a*stride[0]];
        }
        
        float &operator()(int a, int b) {
            return data[a*stride[0] + b*stride[1]];
        }
        
        float &operator()(int a, int b, int c) {
            return data[a*stride[0] + b*stride[1] + c*stride[2]];
        }
        
        float &operator()(int a, int b, int c, int d) {
            return data[a*stride[0] + b*stride[1] + c*stride[2] + d*stride[3]];
        }
        
        // Dimensions
        std::vector<uint32_t> size;
        std::vector<uint32_t> stride;
        
        // The point of the start of the first scanline. Public for now
        // for inspection, but don't assume anything about the way data is
        // stored.
        float *data;
        
        // How the data is actually stored
        std::shared_ptr<std::vector<float> > buffer;       
    };

}


#endif
