-- wander around

function explore(callType)

	if (callType == PURPOSE_POLL) then
		if bugstate == bug.active then
			return 0.5;
		else
			return 0;
		end
	elseif (callType == PURPOSE_ACTIVATE) then
		return 0;
	elseif (callType == PURPOSE_ACTION) then
		if	bugState == bug.active then
	
			--choose a random direction
			--advance for a while
			--repeat
			
		end
		return 0
	else
		return 0;
	end
end

Behavior[nextBehaviorIndex] = explore;
BehaviorNames[nextBehaviorIndex] = "explore";
nextBehaviorIndex = nextBehaviorIndex + 1;
