#include "port/native_lobby_state.hpp"
#include <iostream>
int main(){try{
    using Json=nlohmann::ordered_json;
    Json request;std::cin>>request;
    partyboard::net::NativeLobbyState lobby(request.at("asset"),request.at("total"));
    Json results=Json::array();
    for(const auto& operation:request.at("operations")){
        bool accepted=true;
        try{
            const auto kind=operation.at("kind").get<std::string>();
            if(kind=="join")lobby.join(operation.at("peer"),operation.at("name"));
            else if(kind=="leave")lobby.leave(operation.at("peer"));
            else if(kind=="report")lobby.report(operation.at("peer"),operation.at("report"));
            else throw std::invalid_argument("Unknown lobby operation");
        }catch(const std::exception&){accepted=false;}
        results.push_back({{"accepted",accepted},{"snapshot",lobby.snapshot()}});
    }
    std::cout<<results.dump()<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
