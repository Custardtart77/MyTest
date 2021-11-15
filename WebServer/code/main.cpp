#include <unistd.h>
#include "server/webserver.h"
// 服务器的入口函数
int main() {
    /* 守护进程 后台运行 */
    // daemon(1, 0);show

    WebServer server(
        1316, 3, 60000, false,             /* 端口 ET模式 timeoutMs 优雅退出  */
        3306, "root", "369132", "webserver", /* Mysql配置 ：mysql端口号，用户名，密码，数据库名*/
        12, 6, true, 1, 1024);             /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */


    // 启动服务器
    server.Start();
}
