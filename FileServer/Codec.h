#ifndef CODEC_H
#define CODEC_H
#include<iostream>
#include"File.pb.h"
class Codec{
public:
    Codec();
    ~Codec();
    static std::string encode(const google::protobuf::Message& msg);
private:
};


#endif