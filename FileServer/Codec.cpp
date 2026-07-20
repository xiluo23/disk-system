#include"Codec.h"
#include<arpa/inet.h>
Codec::Codec(){
}
Codec::~Codec(){}
std::string Codec::encode(const google::protobuf::Message& msg){
    std::string body = msg.SerializeAsString();

    uint32_t len = htonl(body.size());

    std::string packet;
    packet.append(reinterpret_cast<char*>(&len), 4);
    packet += body;

    return packet;
}