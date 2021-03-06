#ifndef _API_H_
#define _API_H_
#include "ApiServer.h"
#include "MysqlHelper.h"
#include "MD5.h"
#include "Config.h"
#include "Protocol.h"

using namespace mysqlhelper;

class Api;

#ifndef DEFINE_API_STRUCT
#define DEFINE_API_STRUCT
typedef void (Api::*pmf)(struct evhttp_request*);
#endif 

class Api: public ApiServer{
public:
    //构造函数
    Api();
    //析构函数
    ~Api();
    //设置配置文件路径
    void setConfig(const char* path);
private:
    //api列表
    map<string, pmf> api_list;
    //Mysql
    MysqlHelper* mysql;
    //初始化API列表
    void initApiList();
    //用户登录
    void user_login(struct evhttp_request* request);
    //用户注册
    void user_register(struct evhttp_request* request);
    //设备列表
    void device_list(struct evhttp_request* request);
    //获取设备基本信息
    void device_info(struct evhttp_request* request);
    //发送按键信息
    void device_keypress(struct evhttp_request* request);
    //视频推流权限认证
    void video_push(struct evhttp_request* request);
    //视频观看权限认证
    void video_play(struct evhttp_request* request);
    //打开相机
    void camera_power(struct evhttp_request* request);
    //调用请求对应的方法
    void call(struct evhttp_request* request, const char* str);
protected:
    //libevent http 请求处理
    void read_cb(struct evhttp_request* request);
    //libevent signal 信号处理
    void signal_cb(evutil_socket_t sig, short events, struct event_base* event);
};

#endif
