//
//  Options.h
//  Hex
//
//  Created by Martin Lane-Smith on 6/14/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//

//optionmacro(name, var, min, max, def)
optionmacro("arbdebug", arbDebug, 0, 1, 0)
optionmacro("RC", rcControl, 0, 1, 0)
optionmacro("moveOK", moveOK, 0, 1, 0)
optionmacro("turnOK", turnOK, 0, 1, 0)

optionmacro("brokerdebug", brokerDebug, 0, 1, 0)
optionmacro("routine", logRoutine, 0, 1, 0)
optionmacro("info", logInfo, 0, 1, 1)
optionmacro("warning", logWarning, 0, 1, 1)
