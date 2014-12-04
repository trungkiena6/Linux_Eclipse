-- determine level of nervousness
-- fight or flight

Angst = 0;

ANGST_DECAY = 30	--seconds
ANGST_BUILD = 5		--seconds

function angst(callType)

	if (callType == PURPOSE_POLL) then
		-- calc angst factor from proximity
		angst = 0;
		for i = 0,7,1 do
			range = proximitysensor[i].range;
		
			--calculate angst scaled to range
			if (range > MAX_PROX_RANGE) then
				--angst = angst + 0;
			elseif (range < MIN_PROX_RANGE) then
				angst = angst + setting.angstfactor;
			else
				weight = (MAX_PROX_RANGE - range) / (MAX_PROX_RANGE - MIN_PROX_RANGE);
				angst = angst + setting.nearangst * weight;
			end
		end
		--average
		angst = angst / 8.0;
		if (angst > 1.0) then angst = 1.0 end
		
		AngstDecayConstant = ANGST_DECAY * 1000 / setting.behloopms;
		AngstBuildConstant = ANGST_BUILD * 1000 / setting.behloopms;

		if (angst > Angst) then Angst = Angst + AngstBuildConstant end
		if (angst < Angst) then Angst = Angst - AngstDecayConstant end
		
		if (Angst > 1.0) then Angst = 1.0; end
		if (Angst < 0) then Angst = 0; end
	end
	return 0;
end

Behavior[nextBehaviorIndex] = angst;
BehaviorNames[nextBehaviorIndex] = "angst";
nextBehaviorIndex = nextBehaviorIndex + 1;
