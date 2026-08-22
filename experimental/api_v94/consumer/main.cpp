#include <bdr/database.hpp>
#include <iostream>
int main(){auto db=bdr::Database::open("./v94_consumer_db");db->put_sync("install","ok");auto v=db->get("install");if(!v||*v!="ok")return 1;db->close();std::cout<<"V94 find_package consumer PASS\n";return 0;}
