#include "../pool/sqlconnpool.hpp"
/*
    1、本地的数据库只能使用环回地址，不能用虚拟机地址。
    2、root用户失败
*/

int main() {
    SqlConnPool::GetInstance()->InitSqlPool("127.0.0.1", 3306, "wx", "123456", "wxdb", 5);
    MYSQL* sql = SqlConnPool::GetInstance()->GetSqlConn();
}
