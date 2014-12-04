-- script run once to set up some lua-only globals
-- these globals only used in Lua
-- globals set by C, created by C calls

ShuttingDown 	= false;

-- BEHAVIOR-RELATED

--behavior function lists
--added to as each chunk loads
Behavior = {}
BehaviorNames = {}
nextBehaviorIndex = 1;

-- some defines

PROX_SECTORS		= 8
SECTOR_WIDTH		= (360.0/PROX_SECTORS)
LEFT_SCAN_LIMIT 	= -90	--180 degree field
RIGHT_SCAN_LIMIT	=  90

MIN_PROX_RANGE		= 20	-- cm - no data closer
MAX_PROX_RANGE 		= 200	-- cm - ignore obstacles further

-- some useful functions

function mathfloor(val)
	return val - val % 1;
end

function mathabs(val)
	if (val < 0) then return -val else return val end
end	

function SectorFromDegrees(degrees)
		adjdegrees = degrees - (SECTOR_WIDTH/2);	--offset for center zero
		sector = adjdegrees / SECTOR_WIDTH;
		while (sector > 7) do sector = sector - 8 end
		while (sector < 0) do sector = sector + 8 end
		return math.floor(sector);
end

function DegreesFromSector(sector)
		return (math.floor(sector) * SECTOR_WIDTH);
end