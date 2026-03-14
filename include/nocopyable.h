#pragma once
/*
nocopyable被继承以后，派生类的对象可以正常构造和析构，但派生类无法进行拷贝构造和赋值操作
*/
class nocopyable{
public:
    nocopyable(const nocopyable&) = delete;
    nocopyable& operator=(const nocopyable&) = delete;
protected:
    nocopyable() = default;
    ~nocopyable() = default;
};