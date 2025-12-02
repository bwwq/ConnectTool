#pragma once
#include <string>
#include <random>
namespace nanoid {
    static const std::string _default_dict = "_-0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    inline std::string generate(int size = 21) {
        if (size <= 0) return "";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 63);
        std::string res;
        res.reserve(size);
        for (int i = 0; i < size; ++i) { res += _default_dict[dis(gen)]; }
        return res;
    }
}
