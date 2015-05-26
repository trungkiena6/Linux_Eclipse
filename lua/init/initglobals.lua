BT = require('/root/lua/behaviortree/behavaor_tree')

ActivityList = {}
NextActivity = 1

CurrentActivity = nil;
CurrentActivityName = 'none'
NewActivity = false;

activate = function(activity_name, activity_function)
	CurrentActivity = activity_function
	CurrentActivityName = activity_name
	Activating(activity_name)
	NewActivity = true
	end
	
update = function()
	(CurrentActivity)()
	end
	