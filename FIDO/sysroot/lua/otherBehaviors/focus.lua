-- determine direction of interest

local closestSector = 0;
local closestWeight = 0;	
		
function focus(callType)

	if (callType == PURPOSE_POLL) then
		if (arbstate ~= bug.active then return 0 end
		-- find closest obstacle
		closestSector = 0;
		closestWeight = 0;
		for i = 0,7,1 do
			range = proximitysensor[i].range;
		
			--calculate weight scaled to range
			if (range > MAX_PROX_RANGE) then
				weight = 0;
			elseif (range < MIN_PROX_RANGE) then
				weight = 0.5;
			else
				weight = (MAX_PROX_RANGE - range) / ((MAX_PROX_RANGE - MIN_PROX_RANGE) * 2);
			end
			--boost for being near direction of motion
			if (math.abs(i - motionsector) < 2) and (arbstate == arbotix.walking) then
				--sector near direction of motion
				weight = 1 - (1 - weight) * 0.5;
			else
				--find the biggest
				if (weight > closestWeight) then
					closestWeight = weight;
					closestSector = i;
				end
			end		
		end		
		if (closestSector == SectorFromDegrees(LastFocusHeading)) then
			--no need for action
			return 0;
		else
			--if (weight > 0.75) then
				--return 0.75;
			--else
				--return weight;
			--end
			return 0;
		end
		
	elseif (callType == PURPOSE_ACTIVATE) then
		FocusAttention(orient.relative, DegreesFromSector(closestSector));		
		if Angst > 0.75 and bugsystemstate == bug.active then
			OrientBody(orient.relative, DegreesFromSector(closestSector));
		end	
		return 0;
	elseif (callType == PURPOSE_ACTION) then
		return 0;
	else
		return 0;
	end
end

Behavior[nextBehaviorIndex] = focus;
BehaviorNames[nextBehaviorIndex] = "focus";
nextBehaviorIndex = nextBehaviorIndex + 1;
