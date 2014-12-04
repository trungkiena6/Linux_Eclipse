-- avoid obstacles

function obstacle(callType)

	if (callType == PURPOSE_POLL) then
		--if moving
		--if obstacle in front
		--make bid
		return 0;
	elseif  (callType == PURPOSE_ACTIVATE) then
		--if close, stop
		return 0;
	elseif  (callType == PURPOSE_ACTION) then
		--find nearest obstacle
		--if close, backup first
		--if far, turn away from obstacle
		--when clear, relax
		return 0;
	else
		return 0;
	end
	
end

Behavior[nextBehaviorIndex] = obstacle;
BehaviorNames[nextBehaviorIndex] = "obstacle";
nextBehaviorIndex = nextBehaviorIndex + 1;
