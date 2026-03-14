#pragma once

#include <iostream>
#include <string>
class Timestamp
{
public:
    Timestamp();
    //防止意外的隐式转换
    explicit Timestamp(int64_t microSecondsSinceEpochArg);
    static Timestamp now();
    std::string toString() const;
private:
    int64_t microSecondsSinceEpoch_;
};