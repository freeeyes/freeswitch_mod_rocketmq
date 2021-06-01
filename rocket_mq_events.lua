--add by freeeyes
--处理rocket_mq传递过来的事件

base_dir = freeswitch.getGlobalVariable("base_dir")
if base_dir == nil then
	base_dir = "/usr/local/freeswitch-1.10.2"
end

package.path = package.path..";/usr/local/freeswitch-1.10.2/share/freeswitch/scripts/?.lua"
require("global_param")
scripts_dir = base_dir .."/share/freeswitch/scripts"
json = dofile(scripts_dir.."/libs/json.lua");
api = freeswitch.API();

function debug(s)
	freeswitch.consoleLog("info", "<ROCKET MQ RECV EVENT>".. s .. "\n")
end

debug("argv[1] value is " .. argv[1]) 

local status, params = pcall(json.decode, argv[1])

if status then 
	debug("name=" .. params["name"])
else
	debug(" invalid args")
	return "-ERR INVALID ARGS.\n"
end