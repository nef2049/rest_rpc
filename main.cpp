#include "iostream"

#define __pointer(...) typename PointerOf<__VA_ARGS__>::Result

//
// meta-programming(元编程)
//
// 1. 本质上是对类型进行计算的函数(普通函数是对值进行的计算)
// 2. 规定圆括号适用于值的计算，尖括号适用于类型的计算，所以元编程都是使用尖括号
// 3. 需要显示地调用Result才是通过函数获取返回的类型
//

// =============== 普通元函数 ===============
template <typename T>
struct PointerOf {
    using Result = T*;
};

template <typename T>
struct Pointer2Of {
    // PointerOf后面的尖括号中存在非具体类型的话，那么PointerOf的内部类型Result就是一个推导类型
    // C++标准要求推导类型前面必须有typename关键字
    using Result = typename PointerOf<typename PointerOf<T>::Result>::Result;
    using Result2 = __pointer(__pointer(T));
};
// =============== 普通元函数 ===============

// =============== 高阶元函数 ===============
template <int N, typename T, template <typename> class Func>
struct Times {
    using Result = typename Func<typename Times<N - 1, T, Func>::Result>::Result;
};

template <typename T, template <typename> class Func>
struct Times<1, T, Func> {

    using Result = typename Func<T>::Result;
};
// =============== 高阶元函数 ===============

// =============== currying ===============
// currying: 元函数接收部分参数返回另一个函数
template <int N>
using CharPointer = Times<N, char, PointerOf>;
// =============== currying ===============

// =============== 一切都是类型 ===============
template <bool V>
struct BoolType {};

template <>
struct BoolType<true> {
    enum { Value = true };
    using Result = BoolType<true>;
};

template <>
struct BoolType<false> {
    enum { Value = false };
    using Result = BoolType<false>;
};

using TrueType = BoolType<true>::Result;
using FalseType = BoolType<false>::Result;

// 判断两个类型是否相等的元函数
template <typename T, typename U>
struct IsEqual {
    using Result = FalseType;
};

template <typename T>
struct IsEqual<T, T> {
    using Result = TrueType;
};

// =============== 一切都是类型 ===============

// =============== 一切也都是函数 ===============
#define __int(value) typename IntType<value>::Result
#define __bool(...) typename BoolType<__VA_ARGS__>::Result
#define __true() typename TrueType::Result
#define __false() typename FalseType::Result
#define __is_eq(...) typename IsEqual<__VA_ARGS__>::Result
// =============== 一切也都是函数 ===============

class Object {
public:
    void echo() {
        std::cout << "hello meta-programming" << std::endl;
    }
};

unsigned short const crc16_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241, 0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1,
    0xC481, 0x0440, 0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40, 0x0A00, 0xCAC1, 0xCB81, 0x0B40,
    0xC901, 0x09C0, 0x0880, 0xC841, 0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40, 0x1E00, 0xDEC1,
    0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41, 0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040, 0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1,
    0xF281, 0x3240, 0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441, 0x3C00, 0xFCC1, 0xFD81, 0x3D40,
    0xFF01, 0x3FC0, 0x3E80, 0xFE41, 0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840, 0x2800, 0xE8C1,
    0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41, 0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640, 0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0,
    0x2080, 0xE041, 0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240, 0x6600, 0xA6C1, 0xA781, 0x6740,
    0xA501, 0x65C0, 0x6480, 0xA441, 0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41, 0xAA01, 0x6AC0,
    0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840, 0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40, 0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1,
    0xB681, 0x7640, 0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041, 0x5000, 0x90C1, 0x9181, 0x5140,
    0x9301, 0x53C0, 0x5280, 0x9241, 0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440, 0x9C01, 0x5CC0,
    0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40, 0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40, 0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0,
    0x4C80, 0x8C41, 0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641, 0x8201, 0x42C0, 0x4380, 0x8341,
    0x4100, 0x81C1, 0x8081, 0x4040};
static inline unsigned short crc16_byte(unsigned short crc, const unsigned char data) {
    return (crc >> 8) ^ crc16_table[(crc ^ data) & 0xff];
}

unsigned short _crc16(unsigned short crc, unsigned char const* buffer, unsigned int len) {
    while (len--)
        crc = crc16_byte(crc, *buffer++);
    return crc;
}

// “tlp/int/IntType.h”

template <int V>
struct IntType {
    enum { Value = V };
    using Result = IntType<V>;
};

#include "tuple"

int main() {

    {  // tuple::tuple: Constructs a tuple object. This involves individually constructing its elements,
        // with an initialization that depends on the constructor form invoke
        std::tuple<int, char> first;                            // default
        std::tuple<int, char> second(first);                    // copy
        std::tuple<int, char> third(std::make_tuple(20, 'b'));  // move
        std::tuple<long, char> fourth(third);                   // implicit conversion
        std::tuple<int, char> fifth(10, 'a');                   // initialization
        std::tuple<int, char> sixth(std::make_pair(30, 'c'));   // from pair / move
        std::tuple<int, float, char, std::string> f(100, 12.2f, '#', "this is Sleepy Zeo");

        std::cout << "sixth contains: " << std::get<0>(f) << "  " << std::get<1>(f) << "  " << std::get<2>(f) << "  "
                  << std::get<3>(f) << std::endl;
    }

    unsigned char* buffer = new unsigned char[4];
    buffer[0]=9;
    buffer[1]=0;
    buffer[2]=0;
    buffer[3]=0;
    std::cout<<((unsigned short )(_crc16(0x0000,buffer,4)))<<std::endl;
}
