#include "Client.h"

Client* client_ptr;

Client::Client(){
    client_ptr = this;
}

Client::~Client(){
    
}

string Client::getMacAddress(){
    string mac;
    struct ifreq        ifr;
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket error");
    }
    strncpy(ifr.ifr_name,WLAN_NAME, strlen(WLAN_NAME));  
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == 0) {
        if(ifr.ifr_hwaddr.sa_data != NULL){
            char* mac_c = new char[strlen(ifr.ifr_hwaddr.sa_data)+1];
            sprintf(mac_c,"%02x:%02x:%02x:%02x:%02x:%02x", ifr.ifr_hwaddr.sa_data[0]&0xff, ifr.ifr_hwaddr.sa_data[1]&0xff, ifr.ifr_hwaddr.sa_data[2]&0xff, ifr.ifr_hwaddr.sa_data[3]&0xff, ifr.ifr_hwaddr.sa_data[4]&0xff, ifr.ifr_hwaddr.sa_data[5]&0xff);
            mac = string(mac_c);
        }
    }
    return mac;
}

void Client::sendDeviceInfo(struct bufferevent * bufEvent){
    Json::Value root;
    Json::Value data;
    //获取MAC地址
    data["mac"] = this->getMacAddress();
    data["name"] = DEVICE_NAME;
    root["protocol"] = API_DEVICE_INFO;
    root["data"] = data;
    string json = root.toStyledString();
    bufferevent_write(bufEvent, json.c_str(), json.length());
}

void Client::responseHandler(struct bufferevent * bufEvent, void * args){
    //获取输入缓存
    struct evbuffer * pInput = bufferevent_get_input(bufEvent);
    //获取输入缓存数据的长度
    int nLen = evbuffer_get_length(pInput);
    //获取数据的地址
    const char * pBody = (const char *)evbuffer_pullup(pInput, nLen);
    printf("this Service response data:%s\n",pBody );
    //进行数据处理
    //写到输出缓存,由bufferevent的可写事件读取并通过fd发送
    //bufferevent_write(bufEvent, pResponse, nResLen);

    return ;
}
void Client::requestHandler(struct bufferevent * bufEvent, void * args){
    struct evbuffer *output = bufferevent_get_output(bufEvent);
    //当输出缓冲区的内容长度为0，即全部输出之后
    if (evbuffer_get_length(output) == 0) {
        printf("flushed answer\n");
    }else{
        printf("All data push not completed\n");
    }
}
void Client::eventHandler(struct bufferevent * bufEvent, short sEvent, void * args){
    //请求的连接过程已经完成
    if(BEV_EVENT_CONNECTED == sEvent){
        bufferevent_enable(bufEvent, EV_READ);
        printf("accept success !!!\n");
        //设置读超时时间 10s
        struct timeval tTimeout = {10, 0};
        bufferevent_set_timeouts( bufEvent, &tTimeout, NULL);
        //发送基本信息
        client_ptr->sendDeviceInfo(bufEvent);
    }
    //写操作发生事件
    if(BEV_EVENT_WRITING & sEvent){
        printf("write event !!!\n");
    }
    //结束指示,操作时发生错误
    if (sEvent & (BEV_EVENT_ERROR | BEV_EVENT_EOF)){
        printf("error or end  event !!!\n");
        event_base_loopexit(client_ptr->baseEvent, NULL);  
    }
    //读取发生事件或者超时处理
    if(0 != (sEvent & (BEV_EVENT_TIMEOUT|BEV_EVENT_READING)) ){
        //发送心跳包
        //
        //重新注册可读事件
        bufferevent_enable(bufEvent, EV_READ);
    }
    return ;
}
void Client::start(const char* ip,unsigned int port){

    //创建事件驱动句柄
    this->baseEvent = event_base_new();
    //创建socket类型的bufferevent
    this->bufferEvent = bufferevent_socket_new(this->baseEvent, -1, 0);

    //设置回调函数, 及回调函数的参数
    bufferevent_setcb(this->bufferEvent, responseHandler, requestHandler, eventHandler, NULL);
    //构造服务器地址
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET; 
    sin.sin_addr.s_addr = inet_addr(ip);
    sin.sin_port = htons(port); 
    //连接服务器
    if( bufferevent_socket_connect(this->bufferEvent, (struct sockaddr*)&sin, sizeof(sin)) < 0){
        printf("create socket connect faild !!\n");
        return;
    }
    //开始事件循环
    event_base_dispatch(this->baseEvent);
    //事件循环结束 资源清理
    bufferevent_free(this->bufferEvent);
    event_base_free(this->baseEvent);
}