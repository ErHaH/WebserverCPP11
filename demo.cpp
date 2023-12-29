#include "src/pool/sqlconnpool.hpp"

int main() {
    // const char* host, unsigned port, 
    // const char* user, const char* passwd, 
    // const char* dbname, unsigned maxcount
    SqlConnPool::GetInstance()->InitSqlPool("192.168.40.130", 3306, "root", "123456", "wxdb", 15);
    MYSQL* sql = SqlConnPool::GetInstance()->GetSqlConn();
    
    return 0;
}
