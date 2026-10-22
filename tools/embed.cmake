# a file as a C++ byte array: -DIN=<file> -DOUT=<source> -DNAME=<symbol>, NAME holds the bytes, NAMESize their count
file(READ ${IN} hex HEX)
string(LENGTH "${hex}" digits)
math(EXPR size "${digits} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string(REGEX REPLACE "((0x..,){32})" "\\1\n" bytes "${bytes}")
file(WRITE ${OUT} "#include <cstddef>\n#include <cstdint>\nextern const uint8_t ${NAME}[] = {\n${bytes}\n};\n"
                  "extern const size_t ${NAME}Size = ${size};\n")
