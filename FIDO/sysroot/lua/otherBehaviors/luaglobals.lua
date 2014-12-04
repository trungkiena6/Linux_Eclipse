--LUA Globals

battery 				= {};
battery["unknown"]		= 0;
battery["normal"]		= 1;
battery["low"]			= 2;
battery["shutdown"]		= 3;
batteryvolts			= 0;
batterystate	= battery.unknown;
	
system 					= {};
system["unknown"]		= 0;
system["off"]			= 1;
system["deepsleep"]		= 2;
system["sleeping"]		= 3;
system["resting"]		= 4;
system["standby"]		= 5;
system["active"]		= 6;
systemstate		= system.unknown;

bug 					= {};
bug["unknown"]			= 0;
bug["piconly"]			= 1;
bug["picprox"]			= 2;
bug["BBBon"]			= 3;
bug["poweroff"]			= 4;
bug["AXoff"]			= 5;
bug["AXon"]				= 6;
bug["ready"]			= 7;
bug["standby"]			= 8;
bug["active	"]			= 9;
bugstate		= bug.unknown;
	
arbotix 				= {};
arbotix["relaxed"]		= 0;
arbotix["torqued"]		= 1;
arbotix["ready"]		= 2;
arbotix["walking"]		= 3;	
arbotix["posing"]		= 4;
arbotix["stopping"]		= 5;
arbotix["sitting"]		= 6;
arbotix["standing"]		= 7;
arbotix["relaxing"]		= 8;
arbotix["torqueing"]	= 9;
arbotix["unknown"]		= 10;
arbotix["offline"]		= 11;
arbotix["timeout"]		= 12;
arbotix["error"]		= 13;
arbstate		= arbotix.offline;
	
--setting["<name>"]
--option["<name>"]

msgdirection				= 0;
proximity					= {};
proximity["unknown"]		= 0;
proximity["clear"]			= 1;
proximity["far"]			= 2;
proximity["near"]			= 3;
proximity["approaching"]	= 4;
proximity["rawsonar"]		= 5;
proximity["rawIR"]			= 6;
proximity["rawPIR"]			= 7;
direction 					= {};
direction["front"]			= 0;
direction["front_right"]	= 1;
direction["right"]			= 2;
direction["rear_right"]		= 3;
direction["rear"]			= 4;
direction["rear_left"]		= 5;
direction["left"]			= 6;
direction["front_left"]		= 7;
proximitysensor = {};
for i = 0,15,2 do
	proximitysensor[i] = {};
	proximitysensor[i]["range"]			= 0;
	proximitysensor[i]["status"]		= proximity.unknown;
	proximitysensor[i]["confidence"]	= 0;
	proximitysensor[i]["direction"]		= i;
end

pose = {};
pose["heading"] 			= {};
pose["heading"]["degrees"]	= 0;
pose["heading"]["variance"]	= 0;
pose["pitch"] 				= {};
pose["pitch"]["degrees"]	= 0;
pose["pitch"]["variance"]	= 0;
pose["roll"] 				= {};
pose["roll"]["degrees"]		= 0;
pose["roll"]["variance"]	= 0;
pose["northing"] 			= {};
pose["northing"]["cm"]		= 0;
pose["northing"]["variance"]= 0;
pose["easting"] 			= {};
pose["easting"]["cm"]		= 0;
pose["easting"]["variance"]	= 0;

move 						= {};
move["relative"]			= 0;
move["compass"]				= 1;
move["absolute"]			= 2;

orient						= {};
orient["relative"]			= 0;
orient["compass"]			= 1;

--Behavior[<index>]()
--BehaviorNames[<index>]
--Script[<index>]()
--ScriptNames[<index>]
