#include <switch.h>
#include <unistd.h>
#include <string>

#include "CProducer.h"
#include "CMessage.h"
#include "CSendResult.h"
#include "CErrorMessage.h"
#include "CPushConsumer.h"
#include "CMessageExt.h"

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
CPushConsumer* g_consumer = nullptr;

//配置文件信息
class CRocketMQ_config
{
public:
	std::string producer_name = "";
	std::string produce_ServerAddress = "";	
	std::string produce_topic = "";

	std::string consumer_name = "";
	std::string consumer_ServerAddress = "";	
	std::string consumer_topic = "";

	std::string accessKey = "";
	std::string secretKey = "";

	void print_info()
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[CRocketMQ_config]producer_name=%s!\n", producer_name.c_str());
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[CRocketMQ_config]produce_ServerAddress=%s!\n", produce_ServerAddress.c_str());
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[CRocketMQ_config]produce_topic=%s!\n", produce_topic.c_str());
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[CRocketMQ_config]consumer_name=%s!\n", consumer_name.c_str());
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[CRocketMQ_config]consumer_ServerAddress=%s!\n", consumer_ServerAddress.c_str());
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[CRocketMQ_config]consumer_topic=%s!\n", consumer_topic.c_str());		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[CRocketMQ_config]accessKey=%s!\n", accessKey.c_str());
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[CRocketMQ_config]secretKey=%s!\n", secretKey.c_str());				
	}
};

CRocketMQ_config g_rocket_mq_config;

//当前clinet是否可使用状态
bool g_rocket_mq_can_use = false;

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

//异步发送成功的回调
void SendSuccessCallback(CSendResult result) 
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[SendSuccessCallback]msgid=%s is send ok!\n", result.msgId);
}

//异步发送失败的回调
void SendExceptionCallback(CMQException e) 
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[SendExceptionCallback]error:%d, msg:%s, file:%s:%d!\n", e.error, e.msg, e.file, e.line);
}

//处理消费的消息
int do_consume_message(struct CPushConsumer *consumer, CMessageExt *msgExt)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[do_consume_message]MsgTopic=%s!\n", GetMessageTopic(msgExt));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[do_consume_message]MessageTags=%s!\n", GetMessageTags(msgExt));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[do_consume_message]Keys=%s!\n", GetMessageKeys(msgExt));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[do_consume_message]MessageBody=%s!\n", GetMessageBody(msgExt));
 
    return E_CONSUME_SUCCESS;
}

//链接远程的rocketmq服务器(消费者)
SWITCH_DECLARE(bool) connect_rocketmq_server_consumer(std::string rocket_mq_group_name, std::string rocket_mq_server_info)
{
	g_consumer = CreatePushConsumer(rocket_mq_group_name.c_str());
	if(nullptr == g_consumer)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[connect_rocketmq_server_consumer]g_producer is null!\n");
		return false;
	}

	SetPushConsumerNameServerAddress(g_consumer, rocket_mq_server_info.c_str());
	SetPushConsumerSessionCredentials(g_consumer,  g_rocket_mq_config.accessKey.c_str(), g_rocket_mq_config.secretKey.c_str(), g_rocket_mq_config.consumer_topic.c_str());
	Subscribe(g_consumer, "sip-freeswitch-esl-topic", "*");

	RegisterMessageCallback(g_consumer, do_consume_message);

	int start_connect_return = StartPushConsumer(g_consumer);
	if(start_connect_return == 0)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[connect_rocketmq_server_consumer]load rocketmq success!\n");
		g_rocket_mq_can_use = true;
	}
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[connect_rocketmq_server_consumer]connect rocketmq fail(%d)!\n", start_connect_return);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[connect_rocketmq_server_consumer]err=%s.\n", GetLatestErrorMessage());
		g_rocket_mq_can_use = false;
	}	

	return false;
}

//连接远程的rocketmq服务器(生产者)
SWITCH_DECLARE(bool) connect_rocketmq_server_producer(std::string rocket_mq_group_name, std::string rocket_mq_server_info)
{
	g_producer = CreateProducer(rocket_mq_group_name.c_str());
	if(nullptr == g_producer)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[connect_rocketmq_server_producer]g_producer is null!\n");
		return false;
	}

	//配置Producer的信息
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[connect_rocketmq_server_producer]rocket_mq_server_info=%s!\n", rocket_mq_server_info.c_str());
	SetProducerNameServerAddress(g_producer, rocket_mq_server_info.c_str());
	SetProducerSendMsgTimeout(g_producer, 2);
	SetProducerSessionCredentials(g_producer, g_rocket_mq_config.accessKey.c_str(), g_rocket_mq_config.secretKey.c_str(), g_rocket_mq_config.produce_topic.c_str());	

	//启动生产者
	int start_connect_return = StartProducer(g_producer);
	if(start_connect_return == 0)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[connect_rocketmq_server_producer]load rocketmq success!\n");
		g_rocket_mq_can_use = true;
	}
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[connect_rocketmq_server_producer]connect rocketmq fail(%d)!\n", start_connect_return);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[connect_rocketmq_server_producer]err=%s.\n", GetLatestErrorMessage());
		g_rocket_mq_can_use = false;
	}

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

	if(nullptr != g_consumer)
	{
		ShutdownPushConsumer(g_consumer);
		DestroyPushConsumer(g_consumer);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[close_rocketmq_client]close rocketmq ok!\n");
}

//重连rocketmq服务器
SWITCH_DECLARE(bool) reconnect_rocketmq_servre()
{
	close_rocketmq_client();

	connect_rocketmq_server_producer(g_rocket_mq_config.producer_name, g_rocket_mq_config.produce_ServerAddress);
	connect_rocketmq_server_consumer(g_rocket_mq_config.consumer_name, g_rocket_mq_config.consumer_ServerAddress);

	if(false == g_rocket_mq_can_use)
	{
		return false;
	}
	else
	{
		return true;
	}
}

//发送rocket mq数据
void start_send_message(const char* topic, const char* send_body)
{
	if(false == g_rocket_mq_can_use)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[start_send_message]g_rocket_mq_can_use is disconnect!\n");

		//如果连接不存在，尝试重连
		bool reconnect_return = reconnect_rocketmq_servre();
		if(false == reconnect_return)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[start_send_message]reconnect_rocketmq_servre os fail!\n");
			return;
		}
	}

	CMessage *msg = CreateMessage(topic);
    SetMessageTags(msg, topic);
    SetMessageKeys(msg, topic);

	SetMessageBody(msg, send_body);

	//发送消息(同步)
	CSendResult result;
	SendMessageSync(g_producer, msg, &result);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[start_send_message]result=%d!\n", result.sendStatus);

	//异步发送消息
	//int ret_code = SendMessageAsync(g_producer, msg, SendSuccessCallback, SendExceptionCallback);
	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[start_send_message]ret_code=%d!\n", ret_code);

	DestroyMessage(msg);    
}

//发送rocketmq消息(produce)
SWITCH_STANDARD_API(push_rocketmq_event) 
{
	switch_memory_pool_t *pool;
	char *mycmd = NULL;
	char *argv[2] = {0};
	int argc = 0;

    if (zstr(cmd)) {
        stream->write_function(stream, "[push_rocketmq_event]parameter missing.\n");
        return SWITCH_STATUS_SUCCESS;
    }

    switch_core_new_memory_pool(&pool);
    mycmd = switch_core_strdup(pool, cmd); 

	argc =get_cmd_string_space_count(mycmd);
    if (argc != 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[push_rocketmq_event]parameter number is invalid, mycmd=%s, count=%d.\n", mycmd, argc);
        return SWITCH_STATUS_SUCCESS;
    }

	argc = switch_split(mycmd, ' ', argv);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[push_rocketmq_event]topic=%s.\n", argv[0]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[push_rocketmq_event]message=%s.\n", argv[1]);
	start_send_message(argv[0], argv[1]);

	stream->write_function(stream, "[push_rocketmq_event]put message ok.\n");
    return SWITCH_STATUS_SUCCESS;
}

//读取配置文件
static switch_status_t do_config(CRocketMQ_config& rocket_mq_config)
{
	std::string cf = "rocket_mq.conf";
	switch_xml_t cfg, xml, param;
	switch_xml_t xml_profiles,xml_profile;

	if (!(xml = switch_xml_open_cfg(cf.c_str(), &cfg, NULL))) 
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[do_config]Open of %s failed\n", cf.c_str());
		return SWITCH_STATUS_FALSE;
	}

    if ((xml_profiles = switch_xml_child(cfg, "profiles"))) 
	{
    	for (xml_profile = switch_xml_child(xml_profiles, "profile"); xml_profile;xml_profile = xml_profile->next) 
		{
            if (!(param = switch_xml_child(xml_profile, "param"))) 
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[do_config]No param, check the new config!\n");
                continue;
            }

			for (; param; param = param->next) 
			{
				char *var = (char *) switch_xml_attr_soft(param, "name");
				char *val = (char *) switch_xml_attr_soft(param, "value");

				if (!zstr(var) && !zstr(val)  ) 
				{
					if (!strcasecmp(var, "producer_name")) 
					{
						rocket_mq_config.producer_name = val;
					} 
					else if (!strcasecmp(var, "produce_ServerAddress")) 
					{
						rocket_mq_config.produce_ServerAddress = val;
					} 
					else if (!strcasecmp(var, "produce_topic")) 
					{
						rocket_mq_config.produce_topic = val;
					} 
					else if (!strcasecmp(var, "consumer_name")) 
					{
						rocket_mq_config.consumer_name = val;
					} 
					else if (!strcasecmp(var, "consumer_ServerAddress")) 
					{
						rocket_mq_config.consumer_ServerAddress = val;
					} 
					else if (!strcasecmp(var, "consumer_topic")) 
					{
						rocket_mq_config.consumer_topic =  val;
					}	
					else if (!strcasecmp(var, "accessKey")) 
					{
						rocket_mq_config.accessKey =  val;
					}
					else if (!strcasecmp(var, "secretKey")) 
					{
						rocket_mq_config.secretKey =  val;
					}							
				}			
			}						
		}
	}	

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

//发送rocketmq消息(produce)
SWITCH_STANDARD_API(reconnect_rocketmq_server)
{
	//重新读取配置文件
	do_config(g_rocket_mq_config);

	//重新连接
	reconnect_rocketmq_servre();

	stream->write_function(stream, "[reconnect_rocketmq_server]reconnect ok.\n");
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_rocketmq_load)
{
	switch_api_interface_t* commands_api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(commands_api_interface, "push_rocketmq_event", "push rocketmq message", push_rocketmq_event, "<topic> <message>");
	SWITCH_ADD_API(commands_api_interface, "reconnect_rocketmq_server", "reconnect rocketmq server", reconnect_rocketmq_server, "");

  	/* 读取配置文件 */
	switch_status_t do_config_return = do_config(g_rocket_mq_config);
	if(SWITCH_STATUS_SUCCESS == do_config_return)
	{
		g_rocket_mq_config.print_info();
	}
	else
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[mod_rocketmq_load]do_config is fail!\n");
	}

	connect_rocketmq_server_producer(g_rocket_mq_config.producer_name, g_rocket_mq_config.produce_ServerAddress);
	connect_rocketmq_server_consumer(g_rocket_mq_config.consumer_name, g_rocket_mq_config.consumer_ServerAddress);

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
