#include "port/native_control_mailbox.hpp"
#include <cassert>
#include <thread>
#include <iostream>
using namespace partyboard::net;
int main(){
 NativeControlMailbox box;auto old=box.bind("guest1");
 assert(box.push(old,"old")==NativeControlMailbox::Push::Queued);
 auto current=box.bind("guest1");assert(!box.take());
 assert(box.push(old,"late")==NativeControlMailbox::Push::Retired);
 std::thread producer([&]{for(unsigned i=0;i<16;i++)assert(box.push(current,std::to_string(i))==NativeControlMailbox::Push::Queued);});producer.join();
 for(unsigned i=0;i<16;i++){auto message=box.take();assert(message&&message->route.peer=="guest1"&&message->route.generation==current.generation&&message->text==std::to_string(i));}
 assert(!box.take());assert(box.push(current,"")==NativeControlMailbox::Push::Malformed);
 assert(box.push(current,std::string(65537,'a'))==NativeControlMailbox::Push::Malformed);
 box.retire(current);assert(box.push(current,"late")==NativeControlMailbox::Push::Retired);
 auto replacement=box.bind("guest1");
 for(unsigned i=0;i<4;i++)assert(box.push(replacement,std::string(65536,'a'))==NativeControlMailbox::Push::Queued);
 assert(box.push(replacement,"overflow")==NativeControlMailbox::Push::Overflow);
 box.retire(replacement);box.bind("guest1");bool rejected=false;try{box.take();}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 box.close();assert(box.push(replacement,"late")==NativeControlMailbox::Push::Retired);
 std::cout<<"PASS bounded native control queue: authenticated generations, callback ordering, retirement and sticky loss\n";
}
