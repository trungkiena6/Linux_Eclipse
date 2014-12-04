//
//  Settings.h
//  Hex
//
//  Created by Martin Lane-Smith on 6/14/14.
//  Copyright (c) 2014 Martin Lane-Smith. All rights reserved.
//

//settingmacro(name, var, min, max, def)
settingmacro("arbtimeout", arbTimeout, 1, 20, 10)

settingmacro("behloopms", behLoopDelay, 100, 1000, 250)
settingmacro("navloopms", navLoopDelay, 100, 1000, 250)

settingmacro("rotationP", rotationP, 1, 1000, 10)
settingmacro("movementP", movementP, 1, 1000, 10)
settingmacro("maxspeed", maxSpeed, 5, 150, 20)
settingmacro("maxrotatespeed", maxRotateSpeed, 5, 30, 20)

settingmacro("hunger1", hunger1, 5, 15, 10)
settingmacro("hunger0", hunger0, 5, 15, 11)

settingmacro("proxweight", proxWeight, 0, 1, 0.5)
settingmacro("focusweight", focusWeight, 0, 1, 0.5)
settingmacro("fwdweight", fwdWeight, 0, 1, 0.25)
settingmacro("ageweight", ageWeight, 0, 1, 0.75)

settingmacro("angstfactor", angstfactor, 0, 1, 0.2)

settingmacro("moveintervals", moveInterval, 0, 30, 3)
