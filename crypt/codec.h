#ifndef CODEC_H
#define CODEC_H
#include"File.pb.h"
#include <WinSock2.h>
class Codec{
public:
    Codec();
    ~Codec();
    static std::string encode(const google::protobuf::Message& msg);
private:
};


#endif