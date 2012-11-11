/*
 * type_traits.h
 *
 *   Copyright: Copyright 2012 Peter Goodman, all rights reserved.
 *      Author: Peter Goodman
 */

#ifndef GRANARY_TYPE_TRAITS_HPP_
#define GRANARY_TYPE_TRAITS_HPP_

#ifdef __clang__
namespace std {

  template <bool B, class T = void>
  struct enable_if {};

  template <class T>
  struct enable_if<true, T> { typedef T type; };

#define DEF_TRAIT_VALUE(trait_name, default_val) \
    template <typename T> \
      struct trait_name { \
        enum { \
          value = default_val \
        }; \
      };

#define DEF_TRAIT_VALUE_SPECIALIZATION(trait_name, type_name, val) \
    template <> \
      struct trait_name<type_name> { \
        enum { \
          value = val \
        }; \
      };

#define DEF_TRAIT_VALUE_FORWARD(trait_name) \
    template <typename T> \
    struct trait_name<const T> { \
      enum { \
        value = trait_name<T>::value \
      }; \
    }; \
    template <typename T> \
    struct trait_name<volatile T> { \
      enum { \
        value = trait_name<T>::value \
      }; \
    }; \
    template <typename T> \
    struct trait_name<T &> { \
      enum { \
        value = trait_name<T>::value \
      }; \
    }; \
    template <typename T> \
    struct trait_name<T &&> { \
      enum { \
        value = trait_name<T>::value \
      }; \
    };

  DEF_TRAIT_VALUE(is_signed, false)
  DEF_TRAIT_VALUE_FORWARD(is_signed)
  DEF_TRAIT_VALUE_SPECIALIZATION(is_signed, char, true)
  DEF_TRAIT_VALUE_SPECIALIZATION(is_signed, short, true)
  DEF_TRAIT_VALUE_SPECIALIZATION(is_signed, int, true)
  DEF_TRAIT_VALUE_SPECIALIZATION(is_signed, long, true)
  DEF_TRAIT_VALUE_SPECIALIZATION(is_signed, long long, true)

  DEF_TRAIT_VALUE(is_integral, false)
  DEF_TRAIT_VALUE_FORWARD(is_integral)


#define TRAIT_INTEGRAL(base) \
    DEF_TRAIT_VALUE_SPECIALIZATION(is_integral, base, true) \
    DEF_TRAIT_VALUE_SPECIALIZATION(is_integral, unsigned base, true)

  TRAIT_INTEGRAL(char)
  TRAIT_INTEGRAL(short)
  TRAIT_INTEGRAL(int)
  TRAIT_INTEGRAL(long)
  TRAIT_INTEGRAL(long long)

  template <typename T0, typename T1>
  struct is_same {
  public:
      enum {
          value = false
      };
  };

  template <typename T>
  struct is_same<T, T> {
  public:
      enum {
          value = true
      };
  };

  struct true_type { };
  struct false_type { };
}
#else
# include <type_traits>
#endif


#endif /* GRANARY_TYPE_TRAITS_HPP_ */
