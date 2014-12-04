-- main.lua
-- 
-- main behavior entry point chunk

function NullBehavior(callType)
return 0;
end

-- behavior constants
PURPOSE_POLL = 0;
PURPOSE_DEACTIVATE = 1;
PURPOSE_ACTIVATE = 2;
PURPOSE_ACTION = 3;

-- behavior management globals
activeBehavior = NullBehavior;
activePrecedence = 0;
activeName	= "none";

Print(#Behavior.." Loaded Behaviors");

function main()

	if ShuttingDown then
		-- already shutting down
		return
	end

	-- scan all behaviors to check activation conditions

	nextBehavior = NullBehavior;
	nextPrecedence = 0;
	nextName = "None";
	bid = 0;

	--Print("Main: "..#Behavior.." Loaded Behaviors");

	for bIndex = 1, #Behavior do
		-- iterate all behaviors
--		Print("polling "..BehaviorNames[bIndex]);
		bid = Behavior[bIndex](PURPOSE_POLL);
		--Print("overmind: "..BehaviorNames[bIndex].." reply "..bid);
		if bid and bid > nextPrecedence then
			nextPrecedence = bid;
			nextBehavior = Behavior[bIndex];
			nextName = BehaviorNames[bIndex];
		end
	end

	if (nextPrecedence > activePrecedence) and (activeBehavior ~= nextBehavior) then
		activeBehavior(PURPOSE_DEACTIVATE);
		activeBehavior = nextBehavior;
		activePrecedence = nextPrecedence;
		activeName = nextName;
		Activating(nextName);
		activeBehavior(PURPOSE_ACTIVATE);
	end

	-- regular call to active behavior
--	Print("action "..nextName);
	activePrecedence = activeBehavior(PURPOSE_ACTION);
--	Print("overmind: returned "..nextName);

end

