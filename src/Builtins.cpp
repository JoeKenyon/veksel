
#include "Builtins.h"

#include <libtcc.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

namespace veksel
{
    namespace
    {
        struct Builtin
        {
            const char* name;
            const char* decl;
            void* addr;
        };

        template <typename Fn>
        void* addrOf(Fn fn)
        {
            return reinterpret_cast<void*>(fn);
        }

#define F1(fn) { #fn, "float " #fn "(float);", addrOf(static_cast<float (*)(float)>(&::fn)) }
#define F2(fn) { #fn, "float " #fn "(float,float);", addrOf(static_cast<float (*)(float, float)>(&::fn)) }
#define D1(fn) { #fn, "double " #fn "(double);", addrOf(static_cast<double (*)(double)>(&::fn)) }
#define D2(fn) { #fn, "double " #fn "(double,double);", addrOf(static_cast<double (*)(double, double)>(&::fn)) }

#if defined(__x86_64__) || defined(_M_X64)

        float u64ToF32(unsigned long long v)
        {
            return static_cast<float>(v);
        }

        double u64ToF64(unsigned long long v)
        {
            return static_cast<double>(v);
        }

        unsigned long long f32ToU64(float v)
        {
            return static_cast<unsigned long long>(v);
        }

        unsigned long long f64ToU64(double v)
        {
            return static_cast<unsigned long long>(v);
        }

        long long f32ToI64(float v)
        {
            return static_cast<long long>(v);
        }

        long long f64ToI64(double v)
        {
            return static_cast<long long>(v);
        }

#endif

        const Builtin kBuiltins[] =
        {
            // float math
            F1(sinf), F1(cosf), F1(tanf),
            F1(asinf), F1(acosf), F1(atanf),
            F1(sinhf), F1(coshf), F1(tanhf),
            F1(expf), F1(exp2f),
            F1(logf), F1(log2f), F1(log10f),
            F1(sqrtf), F1(fabsf),
            F1(floorf), F1(ceilf),
            F1(roundf), F1(truncf),

            F2(powf), F2(fmodf), F2(atan2f),
            F2(fminf), F2(fmaxf), F2(hypotf),

            // double math
            D1(sin), D1(cos), D1(tan),
            D1(asin), D1(acos), D1(atan),
            D1(sinh), D1(cosh), D1(tanh),
            D1(exp), D1(exp2),
            D1(log), D1(log2), D1(log10),
            D1(sqrt), D1(fabs),
            D1(floor), D1(ceil),
            D1(round), D1(trunc),

            D2(pow), D2(fmod), D2(atan2),
            D2(fmin), D2(fmax), D2(hypot),

            // standard functions
            {
                "abs",
                "int abs(int);",
                addrOf(static_cast<int (*)(int)>(&::abs))
            },

            {
                "rand",
                "int rand(void);",
                addrOf(&::rand)
            },

            {
                "srand",
                "void srand(unsigned int);",
                addrOf(&::srand)
            },

            {
                "memset",
                "void* memset(void*, int, __SIZE_TYPE__);",
                addrOf(static_cast<void* (*)(void*, int, size_t)>(&::memset))
            },

            {
                "memcpy",
                "void* memcpy(void*, const void*, __SIZE_TYPE__);",
                addrOf(static_cast<void* (*)(void*, const void*, size_t)>(&::memcpy))
            },

#if defined(__x86_64__) || defined(_M_X64)

            // tinycc runtime helpers
            { "__floatundisf", nullptr, addrOf(&u64ToF32) },
            { "__floatundidf", nullptr, addrOf(&u64ToF64) },
            { "__fixunssfdi",  nullptr, addrOf(&f32ToU64) },
            { "__fixunsdfdi",  nullptr, addrOf(&f64ToU64) },
            { "__fixsfdi",     nullptr, addrOf(&f32ToI64) },
            { "__fixdfdi",     nullptr, addrOf(&f64ToI64) },

#endif
        };

#undef F1
#undef F2
#undef D1
#undef D2
    }

    const std::string& builtinDeclarations()
    {
        static const std::string declarations = []
        {
            std::string result =
                "#define M_PI 3.14159265358979323846\n";

            result += "#define RAND_MAX ";
            result += std::to_string(RAND_MAX);
            result += "\n";

            for (const auto& builtin : kBuiltins)
            {
                if (builtin.decl)
                {
                    result += builtin.decl;
                    result += "\n";
                }
            }

            return result;
        }();

        return declarations;
    }

    void addBuiltinSymbols(TCCState* state)
    {
        for (const auto& builtin : kBuiltins)
        {
            tcc_add_symbol(state, builtin.name, builtin.addr);
        }
    }
}
