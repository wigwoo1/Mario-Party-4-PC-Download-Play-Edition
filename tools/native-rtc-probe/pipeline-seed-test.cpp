#include "port/native_pipeline_seed.hpp"
#include <iostream>
int main(int argc,char** argv){try{if(argc!=4)return 2;std::cout<<partyboard::net::installNativePipelineSeed(argv[1],argv[2],argv[3]);return 0;}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
