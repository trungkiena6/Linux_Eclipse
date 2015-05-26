//
//  Settings.h
//  Hex
//
//  Created by Martin Lane-Smith on 6/14/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//

//settingmacro(name, var, min, max, def)

//navigator
settingmacro("navloopms", navLoopDelay, 100, 1000, 500)
settingmacro("imuloopms", imuLoopDelay, 100, 5000, 500)
settingmacro("appReportS", appReportInterval, 1, 60, 10)
settingmacro("KalmanGain", KalmanGain, 0, 1, 0)

//pilot
settingmacro("pilotloopms", pilotLoopDelay, 100, 1000, 500)
settingmacro("arrivalHeading", arrivalHeading, 1, 30, 10)
settingmacro("arrivalRange", arrivalRange, 1, 30, 10)
settingmacro("defSpeed", defSpeed, 10, 100, 50)
settingmacro("defTurnRate", defTurnRate, 10, 100, 50)

//scanner
settingmacro("proxweight", proxWeight, 0, 1, 0.5)
settingmacro("focusweight", focusWeight, 0, 1, 0.5)
settingmacro("fwdweight", fwdWeight, 0, 1, 0.25)
settingmacro("ageweight", ageWeight, 0, 1, 0.75)
settingmacro("contact wt", contactWeight, 0, 1, 0.5)
settingmacro("close wt", closeWeight, 0, 1, 0.3)
settingmacro("distant wt", distantWeight, 0, 1, 0.2)
//
settingmacro("behloopms", behLoopDelay, 100, 1000, 250)

//LUA
settingmacro("moveinterval", moveInterval, 0, 30, 3)
