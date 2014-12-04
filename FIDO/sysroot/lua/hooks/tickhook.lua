

lastSysState = 0;
lastBugState = 0;

function tickhook()
	-- Hook called after Tick message received with new system and bug states

	if bugstate == bug.BBBon then
		-- first call with bbb running
		-- overmind to take over power control
		ChangeBugState(bug.AXoff);	-- no effect on power state
		Info("overmind: started");
	else
		if bugstate ~= lastBugState then
			--Info("tick: bugstate = "..bugstate);
			lastBugState = bugstate;
		end
		if systemstate ~= lastSysState then
			--Info("tick: systemstate = "..systemstate);
			lastSysState = systemstate;
		end	
	end

end
