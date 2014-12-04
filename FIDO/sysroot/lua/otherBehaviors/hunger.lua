-- maintain level of 'hunger' variable
-- test voltage when not moving

Hunger			= 0.0;

function hunger(callType)

	if (callType == PURPOSE_POLL) then
		-- calculate 'hunger' level
		if bugstate ~= bug.active then
			if batteryvolts < setting.hunger1 then
				hunger = 1.0;
			elseif batteryvolts > setting.hunger0 then
				hunger = 0.0;
			else
				hunger = (batteryvolts - setting.hunger1) / (setting.hunger0 - setting.hunger1);
			end
			Hunger = hunger;
		end
		return 0;
	end
	return 0;
end

Behavior[nextBehaviorIndex] = hunger;
BehaviorNames[nextBehaviorIndex] = "hunger";
nextBehaviorIndex = nextBehaviorIndex + 1;
