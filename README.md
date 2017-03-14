# Varstruct Overview

Varstruct creates struct-like types with runtime-computed array sizes.

Varstruct is a header-only library -- if you can't use the included Bazel-based
build config, just add varstruct.h and varstruct_internal.h to your includes.

C-style struct definitions may have internal array fields; however, these field
declarations must specify the size of each array at compile time so that the
compiler may reserve enough space when computing the offset of each member and
the size of the struct.

Unfortunately, many I/O protocols use headers with internal arrays whose size is
specified or computed based on other header fields. Therefore, code that parses
these headers often has to perform error-prone manual pointer manipulation.

Varstruct is a C++11 library that creates types that may contains internal
arrays, and the sizes of these arrays may be passed in at runtime. Varstruct
then computes offsets to each member, and can also read and modify fields when
given an input pointer.

(Modification is performed via std::memcpy() to avoid memory alignment problems
that would otherwise occur accessing misaligned fields -- Varstruct doesn't add
any padding of its own).

You can run the tests by running the following in the repo root directory (you
will need Google Bazel installed):

bazel test :varstruct_test

See the comments in varstruct.h for more information.

Author: Caleb Raitto

# Disclaimer

This is not an official Google product.

