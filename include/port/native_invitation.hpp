#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>
#include <algorithm>

namespace partyboard::net {
// Browser app.mjs invitation wire format. Private initial codes bind an
// assigned guest; authenticated reconnect descriptions use a separate protocol.
inline void validateNativeInvitation(const nlohmann::ordered_json& packet,const std::string& expectedType){
    const auto peer=packet.at("playerId").get<std::string>();
    const bool suffix=peer.size()>5&&peer.size()<=14&&peer.starts_with("guest")&&peer[5]>='1'&&peer[5]<='9'&&
        std::all_of(peer.begin()+6,peer.end(),[](char c){return c>='0'&&c<='9';});
    if(packet.at("version")!=1||(peer!="guest"&&!suffix)||(expectedType!="offer"&&expectedType!="answer")||
        packet.at("description").at("type")!=expectedType||!packet.at("description").at("sdp").is_string())
        throw std::invalid_argument("Invalid assigned native invitation");
}
inline std::string encodeNativeInvitation(const nlohmann::ordered_json& packet){
    validateNativeInvitation(packet,packet.at("description").at("type").get<std::string>());
    const auto text=packet.dump(-1,' ',true); // ASCII JSON agrees with browser btoa.
    if(text.size()>75000)throw std::invalid_argument("Native invitation exceeds bound");
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for(size_t i=0;i<text.size();i+=3){
        const auto a=uint32_t(uint8_t(text[i])),b=i+1<text.size()?uint32_t(uint8_t(text[i+1])):0,c=i+2<text.size()?uint32_t(uint8_t(text[i+2])):0;
        const auto value=(a<<16)|(b<<8)|c;
        result+=alphabet[(value>>18)&63];result+=alphabet[(value>>12)&63];
        result+=i+1<text.size()?alphabet[(value>>6)&63]:'=';result+=i+2<text.size()?alphabet[value&63]:'=';
    }
    return result;
}
inline nlohmann::ordered_json receiveNativeInvitation(const std::string& code,const std::string& expectedType,const std::string& assignedPeer={}){
    if(code.empty()||code.size()>100000||code.size()%4)throw std::invalid_argument("Bounded native invitation required");
    const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string text;
    for(size_t i=0;i<code.size();i+=4){
        uint32_t value=0;unsigned padding=0;
        for(unsigned j=0;j<4;++j){const char c=code[i+j];
            if(c=='='){if(i+4!=code.size()||j<2)throw std::invalid_argument("Invalid invitation padding");++padding;value<<=6;}
            else {const auto pos=alphabet.find(c);if(pos==std::string::npos||padding)throw std::invalid_argument("Invalid invitation encoding");value=(value<<6)|uint32_t(pos);}
        }
        text+=char((value>>16)&255);if(padding<2)text+=char((value>>8)&255);if(!padding)text+=char(value&255);
    }
    auto packet=nlohmann::ordered_json::parse(text);validateNativeInvitation(packet,expectedType);
    if(!assignedPeer.empty()&&packet.at("playerId")!=assignedPeer)throw std::invalid_argument("Reply belongs to another invitation");
    return packet;
}
}
