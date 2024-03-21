主程序--
    IO复用--√
    线程池--√
    连接池--√
        连接池RAII--√
    HTTP连接类--
    请求类--√
    响应类--
    BUF类--√


知识点
    1、regex在match之后的结果数组中，0索引是整个匹配的内容，1、2...才是对应匹配块
    2、post的Content-Type为application/x-www-form-urlencoded时，请求体中可能含有= + & %号，%号的出现是因为特殊符号作为表单内容时需要转义，如/转为%2f
    3、listen第二参数表示backlog，是内核中排队等待客户端连接的最大队长，listen的作用是开启监听，通过accept可以接收队列中的文件描述符
    4、ET模式需要do while来支撑？
    5、结构体用={0}初始化
    6、int -> string   std::to_string
    7、响应头主要实现返回Connection和Content-type和Content-length
    8、响应头是由空行结束的


debug：
（1）debug ParseRequest方法没有走下去，只响应部分请求
    即while语句不成立（两个条件ReadableBytes和status）
    ReadableBytes不可能不满足，上一层调用中同样debug过ParseRequest返回一直是true
    定位问题点是status
    发现status初始化没有重置导致处理状态一直停留在finish  void HttpRequest::Init()