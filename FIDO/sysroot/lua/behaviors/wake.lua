-- wake up

function wake(callType)

	if (callType == PURPOSE_POLL) then
		if (arbstate ~= arbotix.lowvoltage and batterystatus == battery.normal) then
			-- volts OK
			if systemstate == system.active and bugstate == bug.standby then
				--just switch to active
				ChangeBugState(bug.active) 
				return 0;
			elseif systemstate >= system.standby and bugstate < bug.standby then 
				--need to power up
				Print("wake: power up");
				return 1.0;
			end
		else	--low volts
			Print("wake: low volts");
			return 0;
		end
	elseif (callType == PURPOSE_ACTIVATE) then
		return 1;
	elseif (callType == PURPOSE_ACTION) then
		--powering up
		if bugstate < bug.AXon then
			--need to power on the Arbotix
			ChangeBugState(bug.AXon);
			return 1;
		elseif (systemstate == system.active and bugstate ~= bug.active) then
			-- set active
			ChangeBugState(bug.active) 
			return 0;
		elseif (systemstate == system.standby and bugstate ~= bug.standby) then
			-- set standby
			ChangeBugState(bug.standby) 
			return 0;	
		else
			return 0;		
		end
	else	
		return 0;
	end
end

Behavior[nextBehaviorIndex] = wake;
BehaviorNames[nextBehaviorIndex] = "wake";
nextBehaviorIndex = nextBehaviorIndex + 1;
