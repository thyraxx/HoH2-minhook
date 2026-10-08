.code

extern g_pfn_CloseDriver : qword
ex_CloseDriver proc
    jmp qword ptr [g_pfn_CloseDriver]
ex_CloseDriver endp

extern g_pfn_DefDriverProc : qword
ex_DefDriverProc proc
    jmp qword ptr [g_pfn_DefDriverProc]
ex_DefDriverProc endp

extern g_pfn_DriverCallback : qword
ex_DriverCallback proc
    jmp qword ptr [g_pfn_DriverCallback]
ex_DriverCallback endp

extern g_pfn_DrvGetModuleHandle : qword
ex_DrvGetModuleHandle proc
    jmp qword ptr [g_pfn_DrvGetModuleHandle]
ex_DrvGetModuleHandle endp

extern g_pfn_GetDriverModuleHandle : qword
ex_GetDriverModuleHandle proc
    jmp qword ptr [g_pfn_GetDriverModuleHandle]
ex_GetDriverModuleHandle endp

extern g_pfn_OpenDriver : qword
ex_OpenDriver proc
    jmp qword ptr [g_pfn_OpenDriver]
ex_OpenDriver endp

extern g_pfn_PlaySound : qword
ex_PlaySound proc
    jmp qword ptr [g_pfn_PlaySound]
ex_PlaySound endp

extern g_pfn_PlaySoundA : qword
ex_PlaySoundA proc
    jmp qword ptr [g_pfn_PlaySoundA]
ex_PlaySoundA endp

extern g_pfn_PlaySoundW : qword
ex_PlaySoundW proc
    jmp qword ptr [g_pfn_PlaySoundW]
ex_PlaySoundW endp

extern g_pfn_SendDriverMessage : qword
ex_SendDriverMessage proc
    jmp qword ptr [g_pfn_SendDriverMessage]
ex_SendDriverMessage endp

extern g_pfn_WOWAppExit : qword
ex_WOWAppExit proc
    jmp qword ptr [g_pfn_WOWAppExit]
ex_WOWAppExit endp

extern g_pfn_auxGetDevCapsA : qword
ex_auxGetDevCapsA proc
    jmp qword ptr [g_pfn_auxGetDevCapsA]
ex_auxGetDevCapsA endp

extern g_pfn_auxGetDevCapsW : qword
ex_auxGetDevCapsW proc
    jmp qword ptr [g_pfn_auxGetDevCapsW]
ex_auxGetDevCapsW endp

extern g_pfn_auxGetNumDevs : qword
ex_auxGetNumDevs proc
    jmp qword ptr [g_pfn_auxGetNumDevs]
ex_auxGetNumDevs endp

extern g_pfn_auxGetVolume : qword
ex_auxGetVolume proc
    jmp qword ptr [g_pfn_auxGetVolume]
ex_auxGetVolume endp

extern g_pfn_auxOutMessage : qword
ex_auxOutMessage proc
    jmp qword ptr [g_pfn_auxOutMessage]
ex_auxOutMessage endp

extern g_pfn_auxSetVolume : qword
ex_auxSetVolume proc
    jmp qword ptr [g_pfn_auxSetVolume]
ex_auxSetVolume endp

extern g_pfn_joyConfigChanged : qword
ex_joyConfigChanged proc
    jmp qword ptr [g_pfn_joyConfigChanged]
ex_joyConfigChanged endp

extern g_pfn_joyGetDevCapsA : qword
ex_joyGetDevCapsA proc
    jmp qword ptr [g_pfn_joyGetDevCapsA]
ex_joyGetDevCapsA endp

extern g_pfn_joyGetDevCapsW : qword
ex_joyGetDevCapsW proc
    jmp qword ptr [g_pfn_joyGetDevCapsW]
ex_joyGetDevCapsW endp

extern g_pfn_joyGetNumDevs : qword
ex_joyGetNumDevs proc
    jmp qword ptr [g_pfn_joyGetNumDevs]
ex_joyGetNumDevs endp

extern g_pfn_joyGetPos : qword
ex_joyGetPos proc
    jmp qword ptr [g_pfn_joyGetPos]
ex_joyGetPos endp

extern g_pfn_joyGetPosEx : qword
ex_joyGetPosEx proc
    jmp qword ptr [g_pfn_joyGetPosEx]
ex_joyGetPosEx endp

extern g_pfn_joyGetThreshold : qword
ex_joyGetThreshold proc
    jmp qword ptr [g_pfn_joyGetThreshold]
ex_joyGetThreshold endp

extern g_pfn_joyReleaseCapture : qword
ex_joyReleaseCapture proc
    jmp qword ptr [g_pfn_joyReleaseCapture]
ex_joyReleaseCapture endp

extern g_pfn_joySetCapture : qword
ex_joySetCapture proc
    jmp qword ptr [g_pfn_joySetCapture]
ex_joySetCapture endp

extern g_pfn_joySetThreshold : qword
ex_joySetThreshold proc
    jmp qword ptr [g_pfn_joySetThreshold]
ex_joySetThreshold endp

extern g_pfn_mciDriverNotify : qword
ex_mciDriverNotify proc
    jmp qword ptr [g_pfn_mciDriverNotify]
ex_mciDriverNotify endp

extern g_pfn_mciDriverYield : qword
ex_mciDriverYield proc
    jmp qword ptr [g_pfn_mciDriverYield]
ex_mciDriverYield endp

extern g_pfn_mciExecute : qword
ex_mciExecute proc
    jmp qword ptr [g_pfn_mciExecute]
ex_mciExecute endp

extern g_pfn_mciFreeCommandResource : qword
ex_mciFreeCommandResource proc
    jmp qword ptr [g_pfn_mciFreeCommandResource]
ex_mciFreeCommandResource endp

extern g_pfn_mciGetCreatorTask : qword
ex_mciGetCreatorTask proc
    jmp qword ptr [g_pfn_mciGetCreatorTask]
ex_mciGetCreatorTask endp

extern g_pfn_mciGetDeviceIDA : qword
ex_mciGetDeviceIDA proc
    jmp qword ptr [g_pfn_mciGetDeviceIDA]
ex_mciGetDeviceIDA endp

extern g_pfn_mciGetDeviceIDFromElementIDA : qword
ex_mciGetDeviceIDFromElementIDA proc
    jmp qword ptr [g_pfn_mciGetDeviceIDFromElementIDA]
ex_mciGetDeviceIDFromElementIDA endp

extern g_pfn_mciGetDeviceIDFromElementIDW : qword
ex_mciGetDeviceIDFromElementIDW proc
    jmp qword ptr [g_pfn_mciGetDeviceIDFromElementIDW]
ex_mciGetDeviceIDFromElementIDW endp

extern g_pfn_mciGetDeviceIDW : qword
ex_mciGetDeviceIDW proc
    jmp qword ptr [g_pfn_mciGetDeviceIDW]
ex_mciGetDeviceIDW endp

extern g_pfn_mciGetDriverData : qword
ex_mciGetDriverData proc
    jmp qword ptr [g_pfn_mciGetDriverData]
ex_mciGetDriverData endp

extern g_pfn_mciGetErrorStringA : qword
ex_mciGetErrorStringA proc
    jmp qword ptr [g_pfn_mciGetErrorStringA]
ex_mciGetErrorStringA endp

extern g_pfn_mciGetErrorStringW : qword
ex_mciGetErrorStringW proc
    jmp qword ptr [g_pfn_mciGetErrorStringW]
ex_mciGetErrorStringW endp

extern g_pfn_mciGetYieldProc : qword
ex_mciGetYieldProc proc
    jmp qword ptr [g_pfn_mciGetYieldProc]
ex_mciGetYieldProc endp

extern g_pfn_mciLoadCommandResource : qword
ex_mciLoadCommandResource proc
    jmp qword ptr [g_pfn_mciLoadCommandResource]
ex_mciLoadCommandResource endp

extern g_pfn_mciSendCommandA : qword
ex_mciSendCommandA proc
    jmp qword ptr [g_pfn_mciSendCommandA]
ex_mciSendCommandA endp

extern g_pfn_mciSendCommandW : qword
ex_mciSendCommandW proc
    jmp qword ptr [g_pfn_mciSendCommandW]
ex_mciSendCommandW endp

extern g_pfn_mciSendStringA : qword
ex_mciSendStringA proc
    jmp qword ptr [g_pfn_mciSendStringA]
ex_mciSendStringA endp

extern g_pfn_mciSendStringW : qword
ex_mciSendStringW proc
    jmp qword ptr [g_pfn_mciSendStringW]
ex_mciSendStringW endp

extern g_pfn_mciSetDriverData : qword
ex_mciSetDriverData proc
    jmp qword ptr [g_pfn_mciSetDriverData]
ex_mciSetDriverData endp

extern g_pfn_mciSetYieldProc : qword
ex_mciSetYieldProc proc
    jmp qword ptr [g_pfn_mciSetYieldProc]
ex_mciSetYieldProc endp

extern g_pfn_midiConnect : qword
ex_midiConnect proc
    jmp qword ptr [g_pfn_midiConnect]
ex_midiConnect endp

extern g_pfn_midiDisconnect : qword
ex_midiDisconnect proc
    jmp qword ptr [g_pfn_midiDisconnect]
ex_midiDisconnect endp

extern g_pfn_midiInAddBuffer : qword
ex_midiInAddBuffer proc
    jmp qword ptr [g_pfn_midiInAddBuffer]
ex_midiInAddBuffer endp

extern g_pfn_midiInClose : qword
ex_midiInClose proc
    jmp qword ptr [g_pfn_midiInClose]
ex_midiInClose endp

extern g_pfn_midiInGetDevCapsA : qword
ex_midiInGetDevCapsA proc
    jmp qword ptr [g_pfn_midiInGetDevCapsA]
ex_midiInGetDevCapsA endp

extern g_pfn_midiInGetDevCapsW : qword
ex_midiInGetDevCapsW proc
    jmp qword ptr [g_pfn_midiInGetDevCapsW]
ex_midiInGetDevCapsW endp

extern g_pfn_midiInGetErrorTextA : qword
ex_midiInGetErrorTextA proc
    jmp qword ptr [g_pfn_midiInGetErrorTextA]
ex_midiInGetErrorTextA endp

extern g_pfn_midiInGetErrorTextW : qword
ex_midiInGetErrorTextW proc
    jmp qword ptr [g_pfn_midiInGetErrorTextW]
ex_midiInGetErrorTextW endp

extern g_pfn_midiInGetID : qword
ex_midiInGetID proc
    jmp qword ptr [g_pfn_midiInGetID]
ex_midiInGetID endp

extern g_pfn_midiInGetNumDevs : qword
ex_midiInGetNumDevs proc
    jmp qword ptr [g_pfn_midiInGetNumDevs]
ex_midiInGetNumDevs endp

extern g_pfn_midiInMessage : qword
ex_midiInMessage proc
    jmp qword ptr [g_pfn_midiInMessage]
ex_midiInMessage endp

extern g_pfn_midiInOpen : qword
ex_midiInOpen proc
    jmp qword ptr [g_pfn_midiInOpen]
ex_midiInOpen endp

extern g_pfn_midiInPrepareHeader : qword
ex_midiInPrepareHeader proc
    jmp qword ptr [g_pfn_midiInPrepareHeader]
ex_midiInPrepareHeader endp

extern g_pfn_midiInReset : qword
ex_midiInReset proc
    jmp qword ptr [g_pfn_midiInReset]
ex_midiInReset endp

extern g_pfn_midiInStart : qword
ex_midiInStart proc
    jmp qword ptr [g_pfn_midiInStart]
ex_midiInStart endp

extern g_pfn_midiInStop : qword
ex_midiInStop proc
    jmp qword ptr [g_pfn_midiInStop]
ex_midiInStop endp

extern g_pfn_midiInUnprepareHeader : qword
ex_midiInUnprepareHeader proc
    jmp qword ptr [g_pfn_midiInUnprepareHeader]
ex_midiInUnprepareHeader endp

extern g_pfn_midiOutCacheDrumPatches : qword
ex_midiOutCacheDrumPatches proc
    jmp qword ptr [g_pfn_midiOutCacheDrumPatches]
ex_midiOutCacheDrumPatches endp

extern g_pfn_midiOutCachePatches : qword
ex_midiOutCachePatches proc
    jmp qword ptr [g_pfn_midiOutCachePatches]
ex_midiOutCachePatches endp

extern g_pfn_midiOutClose : qword
ex_midiOutClose proc
    jmp qword ptr [g_pfn_midiOutClose]
ex_midiOutClose endp

extern g_pfn_midiOutGetDevCapsA : qword
ex_midiOutGetDevCapsA proc
    jmp qword ptr [g_pfn_midiOutGetDevCapsA]
ex_midiOutGetDevCapsA endp

extern g_pfn_midiOutGetDevCapsW : qword
ex_midiOutGetDevCapsW proc
    jmp qword ptr [g_pfn_midiOutGetDevCapsW]
ex_midiOutGetDevCapsW endp

extern g_pfn_midiOutGetErrorTextA : qword
ex_midiOutGetErrorTextA proc
    jmp qword ptr [g_pfn_midiOutGetErrorTextA]
ex_midiOutGetErrorTextA endp

extern g_pfn_midiOutGetErrorTextW : qword
ex_midiOutGetErrorTextW proc
    jmp qword ptr [g_pfn_midiOutGetErrorTextW]
ex_midiOutGetErrorTextW endp

extern g_pfn_midiOutGetID : qword
ex_midiOutGetID proc
    jmp qword ptr [g_pfn_midiOutGetID]
ex_midiOutGetID endp

extern g_pfn_midiOutGetNumDevs : qword
ex_midiOutGetNumDevs proc
    jmp qword ptr [g_pfn_midiOutGetNumDevs]
ex_midiOutGetNumDevs endp

extern g_pfn_midiOutGetVolume : qword
ex_midiOutGetVolume proc
    jmp qword ptr [g_pfn_midiOutGetVolume]
ex_midiOutGetVolume endp

extern g_pfn_midiOutLongMsg : qword
ex_midiOutLongMsg proc
    jmp qword ptr [g_pfn_midiOutLongMsg]
ex_midiOutLongMsg endp

extern g_pfn_midiOutMessage : qword
ex_midiOutMessage proc
    jmp qword ptr [g_pfn_midiOutMessage]
ex_midiOutMessage endp

extern g_pfn_midiOutOpen : qword
ex_midiOutOpen proc
    jmp qword ptr [g_pfn_midiOutOpen]
ex_midiOutOpen endp

extern g_pfn_midiOutPrepareHeader : qword
ex_midiOutPrepareHeader proc
    jmp qword ptr [g_pfn_midiOutPrepareHeader]
ex_midiOutPrepareHeader endp

extern g_pfn_midiOutReset : qword
ex_midiOutReset proc
    jmp qword ptr [g_pfn_midiOutReset]
ex_midiOutReset endp

extern g_pfn_midiOutSetVolume : qword
ex_midiOutSetVolume proc
    jmp qword ptr [g_pfn_midiOutSetVolume]
ex_midiOutSetVolume endp

extern g_pfn_midiOutShortMsg : qword
ex_midiOutShortMsg proc
    jmp qword ptr [g_pfn_midiOutShortMsg]
ex_midiOutShortMsg endp

extern g_pfn_midiOutUnprepareHeader : qword
ex_midiOutUnprepareHeader proc
    jmp qword ptr [g_pfn_midiOutUnprepareHeader]
ex_midiOutUnprepareHeader endp

extern g_pfn_midiStreamClose : qword
ex_midiStreamClose proc
    jmp qword ptr [g_pfn_midiStreamClose]
ex_midiStreamClose endp

extern g_pfn_midiStreamOpen : qword
ex_midiStreamOpen proc
    jmp qword ptr [g_pfn_midiStreamOpen]
ex_midiStreamOpen endp

extern g_pfn_midiStreamOut : qword
ex_midiStreamOut proc
    jmp qword ptr [g_pfn_midiStreamOut]
ex_midiStreamOut endp

extern g_pfn_midiStreamPause : qword
ex_midiStreamPause proc
    jmp qword ptr [g_pfn_midiStreamPause]
ex_midiStreamPause endp

extern g_pfn_midiStreamPosition : qword
ex_midiStreamPosition proc
    jmp qword ptr [g_pfn_midiStreamPosition]
ex_midiStreamPosition endp

extern g_pfn_midiStreamProperty : qword
ex_midiStreamProperty proc
    jmp qword ptr [g_pfn_midiStreamProperty]
ex_midiStreamProperty endp

extern g_pfn_midiStreamRestart : qword
ex_midiStreamRestart proc
    jmp qword ptr [g_pfn_midiStreamRestart]
ex_midiStreamRestart endp

extern g_pfn_midiStreamStop : qword
ex_midiStreamStop proc
    jmp qword ptr [g_pfn_midiStreamStop]
ex_midiStreamStop endp

extern g_pfn_mixerClose : qword
ex_mixerClose proc
    jmp qword ptr [g_pfn_mixerClose]
ex_mixerClose endp

extern g_pfn_mixerGetControlDetailsA : qword
ex_mixerGetControlDetailsA proc
    jmp qword ptr [g_pfn_mixerGetControlDetailsA]
ex_mixerGetControlDetailsA endp

extern g_pfn_mixerGetControlDetailsW : qword
ex_mixerGetControlDetailsW proc
    jmp qword ptr [g_pfn_mixerGetControlDetailsW]
ex_mixerGetControlDetailsW endp

extern g_pfn_mixerGetDevCapsA : qword
ex_mixerGetDevCapsA proc
    jmp qword ptr [g_pfn_mixerGetDevCapsA]
ex_mixerGetDevCapsA endp

extern g_pfn_mixerGetDevCapsW : qword
ex_mixerGetDevCapsW proc
    jmp qword ptr [g_pfn_mixerGetDevCapsW]
ex_mixerGetDevCapsW endp

extern g_pfn_mixerGetID : qword
ex_mixerGetID proc
    jmp qword ptr [g_pfn_mixerGetID]
ex_mixerGetID endp

extern g_pfn_mixerGetLineControlsA : qword
ex_mixerGetLineControlsA proc
    jmp qword ptr [g_pfn_mixerGetLineControlsA]
ex_mixerGetLineControlsA endp

extern g_pfn_mixerGetLineControlsW : qword
ex_mixerGetLineControlsW proc
    jmp qword ptr [g_pfn_mixerGetLineControlsW]
ex_mixerGetLineControlsW endp

extern g_pfn_mixerGetLineInfoA : qword
ex_mixerGetLineInfoA proc
    jmp qword ptr [g_pfn_mixerGetLineInfoA]
ex_mixerGetLineInfoA endp

extern g_pfn_mixerGetLineInfoW : qword
ex_mixerGetLineInfoW proc
    jmp qword ptr [g_pfn_mixerGetLineInfoW]
ex_mixerGetLineInfoW endp

extern g_pfn_mixerGetNumDevs : qword
ex_mixerGetNumDevs proc
    jmp qword ptr [g_pfn_mixerGetNumDevs]
ex_mixerGetNumDevs endp

extern g_pfn_mixerMessage : qword
ex_mixerMessage proc
    jmp qword ptr [g_pfn_mixerMessage]
ex_mixerMessage endp

extern g_pfn_mixerOpen : qword
ex_mixerOpen proc
    jmp qword ptr [g_pfn_mixerOpen]
ex_mixerOpen endp

extern g_pfn_mixerSetControlDetails : qword
ex_mixerSetControlDetails proc
    jmp qword ptr [g_pfn_mixerSetControlDetails]
ex_mixerSetControlDetails endp

extern g_pfn_mmDrvInstall : qword
ex_mmDrvInstall proc
    jmp qword ptr [g_pfn_mmDrvInstall]
ex_mmDrvInstall endp

extern g_pfn_mmGetCurrentTask : qword
ex_mmGetCurrentTask proc
    jmp qword ptr [g_pfn_mmGetCurrentTask]
ex_mmGetCurrentTask endp

extern g_pfn_mmTaskBlock : qword
ex_mmTaskBlock proc
    jmp qword ptr [g_pfn_mmTaskBlock]
ex_mmTaskBlock endp

extern g_pfn_mmTaskCreate : qword
ex_mmTaskCreate proc
    jmp qword ptr [g_pfn_mmTaskCreate]
ex_mmTaskCreate endp

extern g_pfn_mmTaskSignal : qword
ex_mmTaskSignal proc
    jmp qword ptr [g_pfn_mmTaskSignal]
ex_mmTaskSignal endp

extern g_pfn_mmTaskYield : qword
ex_mmTaskYield proc
    jmp qword ptr [g_pfn_mmTaskYield]
ex_mmTaskYield endp

extern g_pfn_mmioAdvance : qword
ex_mmioAdvance proc
    jmp qword ptr [g_pfn_mmioAdvance]
ex_mmioAdvance endp

extern g_pfn_mmioAscend : qword
ex_mmioAscend proc
    jmp qword ptr [g_pfn_mmioAscend]
ex_mmioAscend endp

extern g_pfn_mmioClose : qword
ex_mmioClose proc
    jmp qword ptr [g_pfn_mmioClose]
ex_mmioClose endp

extern g_pfn_mmioCreateChunk : qword
ex_mmioCreateChunk proc
    jmp qword ptr [g_pfn_mmioCreateChunk]
ex_mmioCreateChunk endp

extern g_pfn_mmioDescend : qword
ex_mmioDescend proc
    jmp qword ptr [g_pfn_mmioDescend]
ex_mmioDescend endp

extern g_pfn_mmioFlush : qword
ex_mmioFlush proc
    jmp qword ptr [g_pfn_mmioFlush]
ex_mmioFlush endp

extern g_pfn_mmioGetInfo : qword
ex_mmioGetInfo proc
    jmp qword ptr [g_pfn_mmioGetInfo]
ex_mmioGetInfo endp

extern g_pfn_mmioInstallIOProcA : qword
ex_mmioInstallIOProcA proc
    jmp qword ptr [g_pfn_mmioInstallIOProcA]
ex_mmioInstallIOProcA endp

extern g_pfn_mmioInstallIOProcW : qword
ex_mmioInstallIOProcW proc
    jmp qword ptr [g_pfn_mmioInstallIOProcW]
ex_mmioInstallIOProcW endp

extern g_pfn_mmioOpenA : qword
ex_mmioOpenA proc
    jmp qword ptr [g_pfn_mmioOpenA]
ex_mmioOpenA endp

extern g_pfn_mmioOpenW : qword
ex_mmioOpenW proc
    jmp qword ptr [g_pfn_mmioOpenW]
ex_mmioOpenW endp

extern g_pfn_mmioRead : qword
ex_mmioRead proc
    jmp qword ptr [g_pfn_mmioRead]
ex_mmioRead endp

extern g_pfn_mmioRenameA : qword
ex_mmioRenameA proc
    jmp qword ptr [g_pfn_mmioRenameA]
ex_mmioRenameA endp

extern g_pfn_mmioRenameW : qword
ex_mmioRenameW proc
    jmp qword ptr [g_pfn_mmioRenameW]
ex_mmioRenameW endp

extern g_pfn_mmioSeek : qword
ex_mmioSeek proc
    jmp qword ptr [g_pfn_mmioSeek]
ex_mmioSeek endp

extern g_pfn_mmioSendMessage : qword
ex_mmioSendMessage proc
    jmp qword ptr [g_pfn_mmioSendMessage]
ex_mmioSendMessage endp

extern g_pfn_mmioSetBuffer : qword
ex_mmioSetBuffer proc
    jmp qword ptr [g_pfn_mmioSetBuffer]
ex_mmioSetBuffer endp

extern g_pfn_mmioSetInfo : qword
ex_mmioSetInfo proc
    jmp qword ptr [g_pfn_mmioSetInfo]
ex_mmioSetInfo endp

extern g_pfn_mmioStringToFOURCCA : qword
ex_mmioStringToFOURCCA proc
    jmp qword ptr [g_pfn_mmioStringToFOURCCA]
ex_mmioStringToFOURCCA endp

extern g_pfn_mmioStringToFOURCCW : qword
ex_mmioStringToFOURCCW proc
    jmp qword ptr [g_pfn_mmioStringToFOURCCW]
ex_mmioStringToFOURCCW endp

extern g_pfn_mmioWrite : qword
ex_mmioWrite proc
    jmp qword ptr [g_pfn_mmioWrite]
ex_mmioWrite endp

extern g_pfn_mmsystemGetVersion : qword
ex_mmsystemGetVersion proc
    jmp qword ptr [g_pfn_mmsystemGetVersion]
ex_mmsystemGetVersion endp

extern g_pfn_sndPlaySoundA : qword
ex_sndPlaySoundA proc
    jmp qword ptr [g_pfn_sndPlaySoundA]
ex_sndPlaySoundA endp

extern g_pfn_sndPlaySoundW : qword
ex_sndPlaySoundW proc
    jmp qword ptr [g_pfn_sndPlaySoundW]
ex_sndPlaySoundW endp

extern g_pfn_timeBeginPeriod : qword
ex_timeBeginPeriod proc
    jmp qword ptr [g_pfn_timeBeginPeriod]
ex_timeBeginPeriod endp

extern g_pfn_timeEndPeriod : qword
ex_timeEndPeriod proc
    jmp qword ptr [g_pfn_timeEndPeriod]
ex_timeEndPeriod endp

extern g_pfn_timeGetDevCaps : qword
ex_timeGetDevCaps proc
    jmp qword ptr [g_pfn_timeGetDevCaps]
ex_timeGetDevCaps endp

extern g_pfn_timeGetSystemTime : qword
ex_timeGetSystemTime proc
    jmp qword ptr [g_pfn_timeGetSystemTime]
ex_timeGetSystemTime endp

extern g_pfn_timeGetTime : qword
ex_timeGetTime proc
    jmp qword ptr [g_pfn_timeGetTime]
ex_timeGetTime endp

extern g_pfn_timeKillEvent : qword
ex_timeKillEvent proc
    jmp qword ptr [g_pfn_timeKillEvent]
ex_timeKillEvent endp

extern g_pfn_timeSetEvent : qword
ex_timeSetEvent proc
    jmp qword ptr [g_pfn_timeSetEvent]
ex_timeSetEvent endp

extern g_pfn_waveInAddBuffer : qword
ex_waveInAddBuffer proc
    jmp qword ptr [g_pfn_waveInAddBuffer]
ex_waveInAddBuffer endp

extern g_pfn_waveInClose : qword
ex_waveInClose proc
    jmp qword ptr [g_pfn_waveInClose]
ex_waveInClose endp

extern g_pfn_waveInGetDevCapsA : qword
ex_waveInGetDevCapsA proc
    jmp qword ptr [g_pfn_waveInGetDevCapsA]
ex_waveInGetDevCapsA endp

extern g_pfn_waveInGetDevCapsW : qword
ex_waveInGetDevCapsW proc
    jmp qword ptr [g_pfn_waveInGetDevCapsW]
ex_waveInGetDevCapsW endp

extern g_pfn_waveInGetErrorTextA : qword
ex_waveInGetErrorTextA proc
    jmp qword ptr [g_pfn_waveInGetErrorTextA]
ex_waveInGetErrorTextA endp

extern g_pfn_waveInGetErrorTextW : qword
ex_waveInGetErrorTextW proc
    jmp qword ptr [g_pfn_waveInGetErrorTextW]
ex_waveInGetErrorTextW endp

extern g_pfn_waveInGetID : qword
ex_waveInGetID proc
    jmp qword ptr [g_pfn_waveInGetID]
ex_waveInGetID endp

extern g_pfn_waveInGetNumDevs : qword
ex_waveInGetNumDevs proc
    jmp qword ptr [g_pfn_waveInGetNumDevs]
ex_waveInGetNumDevs endp

extern g_pfn_waveInGetPosition : qword
ex_waveInGetPosition proc
    jmp qword ptr [g_pfn_waveInGetPosition]
ex_waveInGetPosition endp

extern g_pfn_waveInMessage : qword
ex_waveInMessage proc
    jmp qword ptr [g_pfn_waveInMessage]
ex_waveInMessage endp

extern g_pfn_waveInOpen : qword
ex_waveInOpen proc
    jmp qword ptr [g_pfn_waveInOpen]
ex_waveInOpen endp

extern g_pfn_waveInPrepareHeader : qword
ex_waveInPrepareHeader proc
    jmp qword ptr [g_pfn_waveInPrepareHeader]
ex_waveInPrepareHeader endp

extern g_pfn_waveInReset : qword
ex_waveInReset proc
    jmp qword ptr [g_pfn_waveInReset]
ex_waveInReset endp

extern g_pfn_waveInStart : qword
ex_waveInStart proc
    jmp qword ptr [g_pfn_waveInStart]
ex_waveInStart endp

extern g_pfn_waveInStop : qword
ex_waveInStop proc
    jmp qword ptr [g_pfn_waveInStop]
ex_waveInStop endp

extern g_pfn_waveInUnprepareHeader : qword
ex_waveInUnprepareHeader proc
    jmp qword ptr [g_pfn_waveInUnprepareHeader]
ex_waveInUnprepareHeader endp

extern g_pfn_waveOutBreakLoop : qword
ex_waveOutBreakLoop proc
    jmp qword ptr [g_pfn_waveOutBreakLoop]
ex_waveOutBreakLoop endp

extern g_pfn_waveOutClose : qword
ex_waveOutClose proc
    jmp qword ptr [g_pfn_waveOutClose]
ex_waveOutClose endp

extern g_pfn_waveOutGetDevCapsA : qword
ex_waveOutGetDevCapsA proc
    jmp qword ptr [g_pfn_waveOutGetDevCapsA]
ex_waveOutGetDevCapsA endp

extern g_pfn_waveOutGetDevCapsW : qword
ex_waveOutGetDevCapsW proc
    jmp qword ptr [g_pfn_waveOutGetDevCapsW]
ex_waveOutGetDevCapsW endp

extern g_pfn_waveOutGetErrorTextA : qword
ex_waveOutGetErrorTextA proc
    jmp qword ptr [g_pfn_waveOutGetErrorTextA]
ex_waveOutGetErrorTextA endp

extern g_pfn_waveOutGetErrorTextW : qword
ex_waveOutGetErrorTextW proc
    jmp qword ptr [g_pfn_waveOutGetErrorTextW]
ex_waveOutGetErrorTextW endp

extern g_pfn_waveOutGetID : qword
ex_waveOutGetID proc
    jmp qword ptr [g_pfn_waveOutGetID]
ex_waveOutGetID endp

extern g_pfn_waveOutGetNumDevs : qword
ex_waveOutGetNumDevs proc
    jmp qword ptr [g_pfn_waveOutGetNumDevs]
ex_waveOutGetNumDevs endp

extern g_pfn_waveOutGetPitch : qword
ex_waveOutGetPitch proc
    jmp qword ptr [g_pfn_waveOutGetPitch]
ex_waveOutGetPitch endp

extern g_pfn_waveOutGetPlaybackRate : qword
ex_waveOutGetPlaybackRate proc
    jmp qword ptr [g_pfn_waveOutGetPlaybackRate]
ex_waveOutGetPlaybackRate endp

extern g_pfn_waveOutGetPosition : qword
ex_waveOutGetPosition proc
    jmp qword ptr [g_pfn_waveOutGetPosition]
ex_waveOutGetPosition endp

extern g_pfn_waveOutGetVolume : qword
ex_waveOutGetVolume proc
    jmp qword ptr [g_pfn_waveOutGetVolume]
ex_waveOutGetVolume endp

extern g_pfn_waveOutMessage : qword
ex_waveOutMessage proc
    jmp qword ptr [g_pfn_waveOutMessage]
ex_waveOutMessage endp

extern g_pfn_waveOutOpen : qword
ex_waveOutOpen proc
    jmp qword ptr [g_pfn_waveOutOpen]
ex_waveOutOpen endp

extern g_pfn_waveOutPause : qword
ex_waveOutPause proc
    jmp qword ptr [g_pfn_waveOutPause]
ex_waveOutPause endp

extern g_pfn_waveOutPrepareHeader : qword
ex_waveOutPrepareHeader proc
    jmp qword ptr [g_pfn_waveOutPrepareHeader]
ex_waveOutPrepareHeader endp

extern g_pfn_waveOutReset : qword
ex_waveOutReset proc
    jmp qword ptr [g_pfn_waveOutReset]
ex_waveOutReset endp

extern g_pfn_waveOutRestart : qword
ex_waveOutRestart proc
    jmp qword ptr [g_pfn_waveOutRestart]
ex_waveOutRestart endp

extern g_pfn_waveOutSetPitch : qword
ex_waveOutSetPitch proc
    jmp qword ptr [g_pfn_waveOutSetPitch]
ex_waveOutSetPitch endp

extern g_pfn_waveOutSetPlaybackRate : qword
ex_waveOutSetPlaybackRate proc
    jmp qword ptr [g_pfn_waveOutSetPlaybackRate]
ex_waveOutSetPlaybackRate endp

extern g_pfn_waveOutSetVolume : qword
ex_waveOutSetVolume proc
    jmp qword ptr [g_pfn_waveOutSetVolume]
ex_waveOutSetVolume endp

extern g_pfn_waveOutUnprepareHeader : qword
ex_waveOutUnprepareHeader proc
    jmp qword ptr [g_pfn_waveOutUnprepareHeader]
ex_waveOutUnprepareHeader endp

extern g_pfn_waveOutWrite : qword
ex_waveOutWrite proc
    jmp qword ptr [g_pfn_waveOutWrite]
ex_waveOutWrite endp

end