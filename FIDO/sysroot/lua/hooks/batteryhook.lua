-- Hook called after Battery message received

function batteryhook()
	-- check for battery shutdown
	if batterystate == battery.shutdown then
		-- stop everything
		if not ShuttingDown then
			Error("overmind: Battery shutdown")
			ChangeBugState(bug.poweroff)
			ShuttingDown = True;
		end
	end

end