#include "Api.h"

Api::Api(){
    this->initApiList();
    this->mysql = new MysqlHelper();
}

Api::~Api(){
    delete this->mysql;
}

void Api::setConfig(const char* path){
    Config config;
    //检测配置文件是否存在
    if(!config.FileExist(path)){
      printf("not find config file\n");
      exit(0);
    }
    //读取配置
    config.ReadFile(path);     
    string api_host = config.Read("SERVER_HOST", api_host);
    string mysql_host = config.Read("MYSQL_HOST", mysql_host);
    string mysql_user = config.Read("MYSQL_USER", mysql_user);
    string mysql_pass = config.Read("MYSQL_PASS", mysql_pass);
    string mysql_db = config.Read("MYSQL_DB", mysql_db);
    int mysql_port = config.Read("MYSQL_PORT", mysql_port);
    int api_port = config.Read("API_PORT", api_port);
    //设置监听
    this->setAddress(api_host.c_str()); 
    this->setPort(api_port);
    //设置mysql信息
    this->mysql->init(mysql_host.c_str(), mysql_user.c_str(), mysql_pass.c_str(), mysql_db.c_str(),"",mysql_port);
    try {
        this->mysql->connect();
    } catch (MysqlHelper_Exception& excep) {
        printf("%s\n",excep.errorInfo.c_str() );
        exit(0);
    }  
}

/**
 * 初始化Api列表
 * @Author   DuanEnJian<backtrack843@163.com>
 * @DateTime 2017-05-08
 */
void Api::initApiList() {
  this->api_list["user_login"] = &Api::user_login;
  this->api_list["user_register"] = &Api::user_register;
  this->api_list["device_list"] = &Api::device_list;
  this->api_list["device_info"] = &Api::device_info;
  this->api_list["device_keypress"] = &Api::device_keypress;
  this->api_list["video_push"] = &Api::video_push;
  this->api_list["video_play"] = &Api::video_play;
  this->api_list["camera_power"] = &Api::camera_power;
}

/**
 * 调用请求对应方法
 * @Author   DuanEnJian<backtrack843@163.com>
 * @DateTime 2017-05-08
 * @param    str                              请求方法
 */
void Api::call(struct evhttp_request* request, const char* str) {
  if(str == NULL){
    evhttp_send_error(request, HTTP_BADREQUEST, 0);
    return;
  }
  string func = string(str, strlen(str));
  if (this->api_list.count(func)) {
    (this->*(this->api_list[func]))(request);
  } else {
    evhttp_send_error(request, HTTP_BADREQUEST, 0);
  }
}

void Api::read_cb(struct evhttp_request* request){
    this->call(request,this->getRequestAction());
}

void Api::signal_cb(evutil_socket_t sig, short events, struct event_base* event){
}

/**
 * 用户登录
 * @Author   DuanEnJian<backtrack843@163.com>
 * @DateTime 2017-05-08
 */
void Api::user_login(struct evhttp_request* request) {
  if (evhttp_request_get_command(request) == EVHTTP_REQ_POST) {
    string sql = "select * from user where ";
    Json::Value root;
    Json::Value data;
    //检查参数
    POST_DATA username = this->getPostData("username");
    POST_DATA password = this->getPostData("password");

    if(username.val.length() == 0 || password.val.length() == 0){
      evhttp_send_error(request, HTTP_BADREQUEST, 0);
      return;
    }
    //查询数据
    sql = sql+"username=\""+username.val+"\" and password=\""+MD5(password.val).toStr()+"\"";
    //查找用户是否存在
    MysqlHelper::MysqlData dataSet = this->mysql->queryRecord(sql);
    if (dataSet.size() != 0) {
      //用户存在创建Token
      root["status"] = Json::Value(true);
      root["message"] = Json::Value("ok");
      data["token"] = this->createToken();
      data["user_id"] = dataSet[0]["id"];
      data["nickname"] = dataSet[0]["nickname"];
      data["head"] = dataSet[0]["head"];
    }else{
      root["status"] = Json::Value(false);
      root["message"] = Json::Value("账号或密码不正确");
    }
    root["data"] = data;
    string json = root.toStyledString();
    this->sendJson(request,json.c_str()); 
  }else {
    evhttp_send_error(request, HTTP_BADREQUEST, 0);
  }
}
/**
 * 用户注册
 * @Author   DuanEnJian<backtrack843@163.com>
 * @DateTime 2017-05-08
 */
void Api::user_register(struct evhttp_request* request) {
  this->sendJson(request,"{\"status\":false,\"message\":\"Not open registration\"}");
}
/**
 * 获取设备列表
 * @Author   DuanEnJian<backtrack843@163.com>
 * @DateTime 2017-05-08
 */
void Api::device_list(struct evhttp_request* request){
  if (evhttp_request_get_command(request) == EVHTTP_REQ_GET) {
    Json::Value root;
    Json::Value data;
    const char* token = evhttp_find_header(this->getRequestHeader(),"token");
    //判断token是否为空
    if(token == NULL){
      evhttp_send_error(request, 401, 0);
      return;
    }
    //检查token是否合法
    if(!this->checkToken(string(token,strlen(token)))){
      evhttp_send_error(request, 401, 0);
      return;
    }
    //查询返回数据
    MysqlHelper::MysqlData dataSet = this->mysql->queryRecord("select * from device");
    root["status"] = Json::Value(true);
    root["message"] = Json::Value("ok");
    if(dataSet.size() != 0){
      for(size_t i=0;i<dataSet.size();++i){
        Json::Value node;
        node["id"] = dataSet[i]["id"];
        node["name"] = dataSet[i]["name"];
        node["mac"] = dataSet[i]["mac"];
        node["online"] = dataSet[i]["online"];
        node["sockfd"] = dataSet[i]["sock_fd"];
        node["status"] = dataSet[i]["status"];
        root["data"].append(node);
      }
    }else{
      root["data"] = data;
    }
    string json = root.toStyledString();
    this->sendJson(request,json.c_str()); 
  }else{
    evhttp_send_error(request, HTTP_BADREQUEST, 0);
  }
}

//获取设备基本信息
void Api::device_info(struct evhttp_request* request){
  if (evhttp_request_get_command(request) == EVHTTP_REQ_GET) {
    Json::Value root;
    const char* token = evhttp_find_header(this->getRequestHeader(),"token");
    const char* sockfd = evhttp_find_header(this->getRequestHeader(),"sockfd");

    //判断token是否为空
    if(token == NULL){
      evhttp_send_error(request, 401, 0);
      return;
    }
    //检查token是否合法
    if(!this->checkToken(string(token,strlen(token)))){
      evhttp_send_error(request, 401, 0);
      return;
    }
    //给device发送信息
    this->sendDeviceData(sockfd,API_DEVICE_BASE_INFO);
    //返回数据
    char re_data[SOCK_PIPE_MAXDATA];
    this->readDeviceData(re_data,SOCK_PIPE_MAXDATA);
    this->sendJson(request,re_data); 
  }else{
    evhttp_send_error(request, HTTP_BADREQUEST, 0);
  }
}
//视频推流权限认证
void Api::video_push(struct evhttp_request* request){
  if (evhttp_request_get_command(request) == EVHTTP_REQ_POST) {
    string sql = "select * from user where ";
    //检查参数
    POST_DATA username = this->getPostData("username");
    POST_DATA password = this->getPostData("password");

    if(username.val.length() == 0 || password.val.length() == 0){
      evhttp_send_error(request, HTTP_NOTFOUND, 0);
      return;
    }
    //查询数据
    sql = sql+"username=\""+username.val+"\" and password=\""+MD5(password.val).toStr()+"\"";
    //查找用户是否存在
    MysqlHelper::MysqlData dataSet = this->mysql->queryRecord(sql);
    if (dataSet.size() != 0) {
      this->sendJson(request,"{\"status\":true}");
    }else{
      evhttp_send_error(request, HTTP_NOTFOUND, 0);
    }
  }else{
    evhttp_send_error(request, HTTP_NOTFOUND, 0);
  }
  
}
//视频观看权限认证
void Api::video_play(struct evhttp_request* request){
  if (evhttp_request_get_command(request) == EVHTTP_REQ_POST) {
    POST_DATA token = this->getPostData("token");
    //判断token是否为空
    if(token.val.length() == 0){
      evhttp_send_error(request, HTTP_NOTFOUND, 0);
      return;
    }
    //检查token是否合法
    if(!this->checkToken(token.val)){
      evhttp_send_error(request, HTTP_NOTFOUND, 0);
      return;
    }
    this->sendJson(request,"{\"status\":true}");
  }else{
    evhttp_send_error(request, HTTP_NOTFOUND, 0);
  }
}
void Api::camera_power(struct evhttp_request* request){
  if (evhttp_request_get_command(request) == EVHTTP_REQ_GET) {
    const char* token = evhttp_find_header(this->getRequestHeader(),"token");
    const char* sockfd = evhttp_find_header(this->getRequestHeader(),"sockfd");
    //判断token是否为空
    if(token == NULL){
      evhttp_send_error(request, 401, 0);
      return;
    }
    //检查token是否合法
    if(!this->checkToken(string(token,strlen(token)))){
      evhttp_send_error(request, 401, 0);
      return;
    }
    //给device发送信息
    this->sendDeviceData(sockfd,API_SET_CAMERA_POWER);
    //返回数据
    char re_data[SOCK_PIPE_MAXDATA];
    this->readDeviceData(re_data,SOCK_PIPE_MAXDATA);
    this->sendJson(request,re_data);  
  }else{
    evhttp_send_error(request, HTTP_BADREQUEST, 0);
  }
}
//发送键盘按键
void Api::device_keypress(struct evhttp_request* request){
  if (evhttp_request_get_command(request) == EVHTTP_REQ_GET) {
    const char* token = evhttp_find_header(this->getRequestHeader(),"token");
    const char* sockfd = evhttp_find_header(this->getRequestHeader(),"sockfd");
    const char* key = evhttp_find_header(this->getRequestHeader(),"key");
    //判断token是否为空
    if(token == NULL){
      evhttp_send_error(request, 401, 0);
      return;
    }
    //检查token是否合法
    if(!this->checkToken(string(token,strlen(token)))){
      evhttp_send_error(request, 401, 0);
      return;
    }
    //给device发送信息
    Json::Value device_data;
    device_data["key"] = key;
    this->sendDeviceData(sockfd,API_DEVICE_KEY_DOWN,&device_data,false);
    //返回数据
    Json::Value root;
    root["status"] = Json::Value(true);
    string json = root.toStyledString();
    this->sendJson(request,json.c_str()); 
  }else{
    evhttp_send_error(request, HTTP_BADREQUEST, 0);
  }
}