#include <switch.h>
#include <unistd.h>
#include <string>

#include "CProducer.h"
#include "CMessage.h"
#include "CSendResult.h"

//处理会议踢人播放一段语音后的问题。
//add by freeeyes

/*
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_example_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_example_runtime);
*/

SWITCH_MODULE_LOAD_FUNCTION(mod_rocketmq_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_rocketmq_shutdown);
SWITCH_MODULE_DEFINITION(mod_rocketmq, mod_rocketmq_load, mod_rocketmq_shutdown,  NULL);

// cmd为参数列表
// sessin为当前callleg的session
// stream为当前输出流。如果想在Freeswitch控制台中输出什么，可以往这个流里写
#define SWITCH_STANDARD_API(name)  \
static switch_status_t name (_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream)

//全局变量
CProducer* g_producer = nullptr;

//判断一个命令行里有多少空格
int get_cmd_string_space_count(const char* cmd)
{
	int arg_count = 1;
	int cmd_size = strlen(cmd);
	int i = 0;
	
	for(i = 0; i < cmd_size; i++)
	{
		if(cmd[i] == ' ')
		{
			arg_count++;
		}
	}

	return arg_count;
}

//连接远程的rocketmq服务器
SWITCH_DECLARE(bool) connect_rocketmq_server(std::string rocket_mq_group_name, std::string rocket_mq_server_info)
{
    g_producer = CreateProducer(rocket_mq_group_name.c_str());
	if(nullptr == g_producer)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[connect_rocketmq_server]g_producer is null!\n");
		return false;
	}

	//配置Producer的信息
    SetProducerNameServerAddress(g_producer, rocket_mq_server_info.c_str());
	SetProducerSendMsgTimeout(g_producer, 2);
	SetProducerSessionCredentials(g_producer, "rocketmq-oncon", "sitech1995", "sip-freeswitch-esl-topic");

    //启动生产者
    int start_connect_return = StartProducer(g_producer);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[connect_rocketmq_server]load rocketmq success(%d)!\n", start_connect_return);

	return true;
}

//关闭远程链接
void close_rocketmq_client()
{
	if(nullptr != g_producer)
	{
		ShutdownProducer(g_producer);
		DestroyProducer(g_producer);
		g_producer = nullptr;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[close_rocketmq_client]close rocketmq ok!\n");
}

//发送rocket mq数据
void start_send_message(const char* topic, const char* send_body)
{
	CSendResult result;
	if(nullptr == g_producer)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[start_send_message]g_producer is nullptr!\n");
		return;
	}

	CMessage *msg = CreateMessage(topic);
    SetMessageTags(msg, topic);
    SetMessageKeys(msg, topic);

	SetMessageBody(msg, send_body);

	//发送消息
	SendMessageSync(g_producer, msg, &result);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[start_send_message]result=%d!\n", result.sendStatus);
    
}

//发送rocketmq消息(produce)
SWITCH_STANDARD_API(conference_push_rocketmq_function) 
{
	switch_memory_pool_t *pool;
	char *mycmd = NULL;
	char *argv[2] = {0};
	int argc = 0;

    if (zstr(cmd)) {
        stream->write_function(stream, "[conference_push_rocketmq_function]parameter missing.\n");
        return SWITCH_STATUS_SUCCESS;
    }

    switch_core_new_memory_pool(&pool);
    mycmd = switch_core_strdup(pool, cmd); 

	argc =get_cmd_string_space_count(mycmd);
    if (argc != 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[conference_push_rocketmq_function]parameter number is invalid, mycmd=%s, count=%d.\n", mycmd, argc);
        return SWITCH_STATUS_SUCCESS;
    }

	argc = switch_split(mycmd, ' ', argv);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[conference_push_rocketmq_function]topic=%s.\n", argv[0]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[conference_push_rocketmq_function]message=%s.\n", argv[1]);
	start_send_message(argv[0], argv[1]);

    return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_rocketmq_load)
{
	switch_api_interface_t* commands_api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(commands_api_interface, "conference_push_rocketmq_function", "push rocketmq message", conference_push_rocketmq_function, "<topic> <message>");

  	/* 读取配置文件 */
	connect_rocketmq_server("test_rocket_mq", "172.18.192.50:9876");

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[mod_rocketmq_load]load rocketmq success!\n");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/* Called when the system shuts down */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_rocketmq_shutdown)
{
	close_rocketmq_client();
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[mod_rocketmq_shutdown]unload rocketmq ok!\n");
	return SWITCH_STATUS_SUCCESS;
}

/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automaticly
SWITCH_MODULE_RUNTIME_FUNCTION(mod_example_runtime);
{
	while(looping)
	{
		switch_yield(1000);
	}
	return SWITCH_STATUS_TERM;
}
*/
