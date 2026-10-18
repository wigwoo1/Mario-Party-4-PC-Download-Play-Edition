#include "port/native_bundled_pipeline_seed.hpp"
#include <iostream>
int main(int argc,char** argv){try{
    if(argc!=2)return 2;
    const auto bundled=partyboard::net::readNativeBundledPipelineSeed(argv[1]);
    std::cout<<nlohmann::json{{"sha256",bundled.snapshot->sha256},{"definitions",bundled.definitions},
        {"bytes",bundled.snapshot->bytes.size()}}.dump();return 0;
}catch(const std::exception& error){std::cerr<<error.what();return 1;}}
