// SpotifyHotkeysCpp.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <initguid.h>
#include <windows.h>
#include <Winuser.h>
#include <psapi.h>
#include <Audioclient.h>
#include <Mmdeviceapi.h>
#include <Audiopolicy.h>
#include <iostream>

#define SAFE_RELEASE(x) if (x) {x->Release(); x=NULL;}

const int HkId_PlayPause = 0x01;
const int HkId_VolumUp = 0x02;
const int HkId_VolumDown = 0x03;
const int HkId_NextSong = 0x04;
const int HkId_PreviousSong = 0x05;

TCHAR SpotifyProcessName[MAX_PATH] = TEXT("Spotify.exe");


//template <class T> void SafeRelease(T **ppT)
//{
//	if (*ppT)
//	{
//		(*ppT)->Release();
//		*ppT = NULL;
//	}
//}


void SendKey(WORD vKey)
{
	INPUT inputs;
	inputs.type = INPUT_KEYBOARD;
	inputs.ki.wVk = vKey;
	inputs.ki.wScan = 0x45;
	inputs.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
	inputs.ki.time = 0;

	SendInput(1, &inputs, sizeof(INPUT));
}

bool IsSpotify(DWORD processID)
{
	TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");

	// Get a handle to the process.
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

	// Get the process name.
	if (NULL != hProcess)
	{
		HMODULE hMod;
		DWORD cbNeeded;

		if (EnumProcessModules(hProcess, &hMod, sizeof(hMod),
			&cbNeeded))
		{
			GetModuleBaseName(hProcess, hMod, szProcessName,
				sizeof(szProcessName) / sizeof(TCHAR));
		}
	}

	CloseHandle(hProcess);
	return _tcscmp(szProcessName, SpotifyProcessName) == 0;
}

HRESULT GetSpotifyAudioVolume(ISimpleAudioVolume** ppVolumeControl)
{
	HRESULT hr = S_OK;

	DWORD aProcesses[1024], cbNeeded, cProcesses;
	unsigned int i;

	if (!EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded))
	{
		return 1; // ERROR
	}

	cProcesses = cbNeeded / sizeof(DWORD);
	for (i = 0; i < cProcesses; i++)
	{
		if (aProcesses[i] != 0 && IsSpotify(aProcesses[i]))
		{
			IMMDevice* pDevice = NULL;
			IMMDeviceEnumerator* pEnumerator = NULL;

			CoInitialize(NULL);

			// get the device enumerator
			hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (LPVOID *)&pEnumerator);
			if (hr != S_OK) return hr;

			// get default audio endpoint
			hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
			if (hr != S_OK) return hr;

			/////////////////////////////////////////////
			IAudioSessionManager2* pSessionManager = NULL;
			hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&pSessionManager);
			if (hr != S_OK) return hr;

			IAudioSessionEnumerator* pSessionEnumerator = NULL;
			hr = pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
			if (hr != S_OK) return hr;
			int count;
			hr = pSessionEnumerator->GetCount(&count);
			if (hr != S_OK) return hr;

			// search for audio session with correct name
			// NOTE: we could also use the process id instead of the app name (with IAudioSessionControl2)
			for (int q = 0; q < count; q++)
			{
				IAudioSessionControl* ctl = NULL;
				hr = pSessionEnumerator->GetSession(q, &ctl);
				if (hr != S_OK) return hr;

				IAudioSessionControl2* ctl2 = NULL;
				hr = ctl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctl2);
				if (hr != S_OK) return hr;


				DWORD cpid;
				hr = ctl2->GetProcessId(&cpid);
				// TODO: fails sometimes, don't know why 
				if (hr != S_OK) continue;

				if (cpid == aProcesses[i])
				{
					LPWSTR sessionInstanceIdentifier;
					ctl2->GetSessionIdentifier(&sessionInstanceIdentifier);
					
					pSessionManager->GetSimpleAudioVolume(NULL, true, &(*ppVolumeControl));
					ctl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&*(ppVolumeControl));
					(*ppVolumeControl)->AddRef();
				}

				// TODO: cleanup

				if (*ppVolumeControl != NULL)
				{
					return S_OK;
				}
			}
		}
	}

	return NULL;
}

void VolumeUp(ISimpleAudioVolume* sav, float delta)
{
	UUID* f = new UUID();
	UuidCreateNil(f);
	for (auto s = 0; s < 10; s++)
	{
		float vol;
		sav->GetMasterVolume(&vol);
		vol += delta / 100.0f;
		sav->SetMasterVolume(vol, NULL);
	}

}

int main()
{
	ISimpleAudioVolume* pSimpleAudioControl = NULL;
	auto hr = GetSpotifyAudioVolume(&pSimpleAudioControl);
	if (hr != S_OK)
	{
		// TODO: error
	}

	RegisterHotKey(nullptr, HkId_PlayPause,		MOD_NOREPEAT | MOD_ALT | MOD_CONTROL, VK_HOME);
	RegisterHotKey(nullptr, HkId_VolumUp,		MOD_ALT | MOD_CONTROL, VK_UP);
	RegisterHotKey(nullptr, HkId_VolumDown,		MOD_ALT | MOD_CONTROL, VK_DOWN);
	RegisterHotKey(nullptr, HkId_NextSong,		MOD_NOREPEAT | MOD_ALT | MOD_CONTROL, VK_RIGHT);
	RegisterHotKey(nullptr, HkId_PreviousSong,	MOD_NOREPEAT | MOD_ALT | MOD_CONTROL, VK_LEFT);

	MSG msg = { 0 };
	while(GetMessage(&msg, nullptr, 0, 0) != 0)
	{ 
		if (msg.message == WM_HOTKEY) {
			switch (msg.wParam)
			{
			case HkId_PlayPause:
				std::cout << "Play/pause" << std::endl;
				SendKey(0xB3);
				break;
			case HkId_VolumUp:
				std::cout << "Volume up" << std::endl;
				VolumeUp(pSimpleAudioControl, 0.1f);
				break;
			case HkId_VolumDown:
				std::cout << "Volume up" << std::endl;
				VolumeUp(pSimpleAudioControl, -0.1f);
				break;
			case HkId_NextSong:
				std::cout << "Next song" << std::endl;
				SendKey(0xB0);
				break;
			case HkId_PreviousSong:
				std::cout << "Next song" << std::endl;
				SendKey(0xB1);
				break;
			}
		}
	}

    return 0;
}

