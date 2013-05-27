#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Common/Shared code for Cadence
# Copyright (C) 2012 Filipe Coelho <falktx@falktx.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# For a full copy of the GNU General Public License see the COPYING file

# ------------------------------------------------------------------------------------------------------------
# Imports (Global)

from PyQt4.QtCore import QProcess, QSettings
from time import sleep

# ------------------------------------------------------------------------------------------------------------
# Imports (Custom Stuff)

from shared import *

# ------------------------------------------------------------------------------------------------------------
# Default Plugin PATHs

DEFAULT_LADSPA_PATH = [
    os.path.join(HOME, ".ladspa"),
    os.path.join("/", "usr", "lib", "ladspa"),
    os.path.join("/", "usr", "local", "lib", "ladspa")
]

DEFAULT_DSSI_PATH = [
    os.path.join(HOME, ".dssi"),
    os.path.join("/", "usr", "lib", "dssi"),
    os.path.join("/", "usr", "local", "lib", "dssi")
]

DEFAULT_LV2_PATH = [
    os.path.join(HOME, ".lv2"),
    os.path.join("/", "usr", "lib", "lv2"),
    os.path.join("/", "usr", "local", "lib", "lv2")
]

DEFAULT_VST_PATH = [
    os.path.join(HOME, ".vst"),
    os.path.join("/", "usr", "lib", "vst"),
    os.path.join("/", "usr", "local", "lib", "vst")
]

# ------------------------------------------------------------------------------------------------------------
# ALSA file-type indexes

iAlsaFileNone  = 0
iAlsaFileLoop  = 1
iAlsaFileJACK  = 2
iAlsaFilePulse = 3
iAlsaFileMax   = 4

# ------------------------------------------------------------------------------------------------------------
# Global Settings

GlobalSettings = QSettings("Cadence", "GlobalSettings")

# ------------------------------------------------------------------------------------------------------------
# Get Process list

def getProcList():
    retProcs = []

    if HAIKU or LINUX or MACOS:
        process = QProcess()
        process.start("ps", ["-u", str(os.getuid())])
        process.waitForFinished()

        processDump = process.readAllStandardOutput().split("\n")

        for i in range(len(processDump)):
            if (i == 0): continue
            dumpTest = str(processDump[i], encoding="utf-8").rsplit(":", 1)[-1].split(" ")
            if len(dumpTest) > 1 and dumpTest[1]:
                retProcs.append(dumpTest[1])

    else:
        print("getProcList() - Not supported in this system")

    return retProcs

# ------------------------------------------------------------------------------------------------------------
# Start ALSA-Audio Bridge, reading its settings

def startAlsaAudioLoopBridge():
    channels = GlobalSettings.value("ALSA-Audio/BridgeChannels", 2, type=int)
    useZita  = bool(GlobalSettings.value("ALSA-Audio/BridgeTool", "alsa_in", type=str) == "zita")

    os.system("cadence-aloop-daemon --channels=%i %s &" % (channels, "--zita" if useZita else ""))

# ------------------------------------------------------------------------------------------------------------
# Stop all audio processes, used for force-restart

def stopAllAudioProcesses():
    if not (HAIKU or LINUX or MACOS):
        print("stopAllAudioProcesses() - Not supported in this system")
        return

    process = QProcess()

    # Tell pulse2jack script to create files, prevents pulseaudio respawn
    process.start("cadence-pulse2jack", "--dummy")
    process.waitForFinished()

    procsTerm = ["a2j", "a2jmidid", "artsd", "jackd", "jackdmp", "knotify4", "lash", "ladishd", "ladiappd", "ladiconfd", "jmcore"]
    procsKill = ["jackdbus", "pulseaudio"]
    tries = 20

    process.start("killall", procsTerm)
    process.waitForFinished()
    process.start("killall", ["-KILL"] + procsKill)
    process.waitForFinished()

    waitProcsDeath(procsTerm + procsKill, tries)

# Helper to wait for death of all processes in list (no matter how — killed, terminated or something else)
# Hint: tries = 0 (default value) results in infinite wait time

def waitProcsDeath(procsWait, tries = 0):
    to_Break = False
    procsList = getProcList()

    for proc in procsWait:
        while proc in procsList:
            sleep(0.1)
            procsList = getProcList()
            tries -= 1
            if tries == 0:
                print("warning: " + proc + " don't want to die, can't wait more")
                to_Break = True
                break
        if to_Break:
            break
