主程序--
    IO复用--√
    线程池--√
    连接池--√
        连接池RAII--√
    HTTP连接类--√
    请求类--√
    响应类--√
    BUF类--√
    log类--


知识点
    1、regex在match之后的结果数组中，0索引是整个匹配的内容，1、2...才是对应匹配块
    2、post的Content-Type为application/x-www-form-urlencoded时，请求体中可能含有= + & %号，%号的出现是因为特殊符号作为表单内容时需要转义，如/转为%2f
    3、listen第二参数表示backlog，是内核中排队等待客户端连接的最大队长，listen的作用是开启监听，通过accept可以接收队列中的文件描述符
    4、ET模式需要do while来支撑？
    5、结构体用={0}初始化
    6、int -> string   std::to_string
    7、响应头主要实现返回Connection和Content-type和Content-length
    8、响应头是由空行结束的


优化点：
（1）视频传输卡顿
（2）新增日志线程信息显示
        string追加int类型数据的方法有to_string以及使用stringstream流接收int

bug：
（1）debug ParseRequest方法没有走下去，只响应部分请求
    即while语句不成立（两个条件ReadableBytes和status）
    ReadableBytes不可能不满足，上一层调用中同样debug过ParseRequest返回一直是true
    定位问题点是status
    发现status初始化没有重置导致处理状态一直停留在finish  void HttpRequest::Init()
    修复

（2）debug UserVerify_方法传参为空，没有存储表单密码
    沿着表单密码变化点找到ParseFromUrlencoded_方法最后处理一个字段时判断条件设置错误
    修复

（3）日志输出引发段错误
    1、检查不是异步队列线程问题
    2、gdb堆栈发现在LOG_INFO宏调用完毕出栈的时候才出现段错误
    3、发现原因是空指针，直接原因是初始化函数中判断条件错误没有进入初始化指针逻辑

（4）日志格式有误（换行+内容）
    根本问题是字符串处理不当，append日志等级信息的时候将整个string附加进去，包括'\0'结束符，导致buf读取到\0中断后续内容读取