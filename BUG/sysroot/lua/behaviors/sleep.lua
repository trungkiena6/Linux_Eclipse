-- go to sleep

function sleep(callType)

	if (callType == PURPOSE_POLL) then
		if (arbstate == arbotix.lowvoltage or batterystatus == battery.shutdown) then
			return 1;
		elseif systemstate == system.standby and bugstate == bug.active then
			--just switch to standby
			ChangeBugState(bug.standby)
			return 0;
		elseif systemstate < system.standby and bugstate > bug.AXoff then
			--arbotix needs to be powered off
			return 0.75;
		elseif systemstate < system.resting then
			--BBB needs to be powered off
			return 0.75;
		elseif batterystate == battery.low then
			return 0.5;
		else
			return 0;
		end
	elseif (callType == PURPOSE_ACTIVATE) then
		return 1;
	elseif (callType == PURPOSE_ACTION) then
		if (arbstate == arbotix.lowvoltage or batterystatus == battery.shutdown) then
			if bugstate > bug.AXon then
			--arbotix needs to be powered off
			ChangeBugState(bug.AXon);
			return 1;
			return 1;
		elseif (systemstate < system.standby or batterystate == battery.low) and bugstate > bug.AXoff then
			--arbotix needs to be powered off
			if bugstate > bug.AXon then
				--stop what you're doing
				ChangeBugState(bug.AXon);
			elseif bugstate == bug.AXon and (arbstate <= arbotix.torqued or arbstate >= arbotix.unknown) then
				ChangeBugState(bug.AXoff);
			end
			return 1;
		elseif systemstate < system.resting and bugstate == bug.AXoff then
			--arbotix ready for shutdown
			Info("sleep: poweroff");
			ChangeBugState(bug.poweroff);
			ShuttingDown = True;
			return 1;
		else
			return 0;
		end
	else
		return 0;
	end
end

Behavior[nextBehaviorIndex] = sleep;
BehaviorNames[nextBehaviorIndex] = "sleep";
nextBehaviorIndex = nextBehaviorIndex + 1;
