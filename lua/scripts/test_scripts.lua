
Forward_10 = BehaviourTree.Task:new({
  run = function()
 	
    reply = Move(move.forward, 10, 0)
    
    if (reply == 'success') success() end
    if (reply == 'running') running() end
    if (reply == 'fail') 	fail() end
    
  end
})






Forward_100 = function() return Move(move.forward, 100, 0) end

Backward_10 = function() return Move(move.backward, 10, 0) end

Backward_100 = function() return Move(move.backward, 100, 0) end

Left_90 = function() return Orient(orient.relative, -90) end

Right_90 = function() return Orient(orient.relative, 90) end

GetAFix = function() return GetFix(30) end

Stop = function() return Move(move.none,0,0) end

ActivityList = {"Forward_100", "Forward_10", "Stop", "Backward_10", "Backward_100", "Left_90", "Right_90", "GetAFix"}
NextActivity = 9