#include "gui.h"

#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>
#include <math.h>
#include <vector>
#include <algorithm>

#include <thread>

#define F5_KEY 0X73
#define F6_KEY 0X75
#define RIGHT_MOUSE 0X02


const uintptr_t dwLocalPlayer = 0xDE997C;
const uintptr_t m_fFlags = 0x104;

const uintptr_t dw_healthOffset = 0x100;
const uintptr_t dw_teamOffset = 0xF4;
const uintptr_t dw_pos = 0x138;

const uintptr_t dw_angRotation = 0x525D0C0;//client

int numOfPlayers = 32;

const uintptr_t PlayerCount = 0x5EC82C; // engine

const uintptr_t EntityPlayer_base = 0x4DFEF0C;
const uintptr_t playerOffset = 0x10;

// engine

const uintptr_t dwClientState = 0x59F19C;
const uintptr_t dwClientState_ViewAngles = 0x4D90;

bool cheat = false;

//client.dll+525D0C0
DWORD procId;



uintptr_t Read(uintptr_t addr, DWORD procId) {
	uintptr_t val;
	Toolhelp32ReadProcessMemory(procId, (LPVOID)addr, &val, sizeof(val), NULL);
	return val;
}

float ReadFloat(uintptr_t addr, DWORD procId) {
	float val;
	Toolhelp32ReadProcessMemory(procId, (LPVOID)addr, &val, sizeof(val), NULL);
	return val;
}

uintptr_t GetModuleBaseAddress(const char* modName) {
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, procId);
	if (hSnap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry)) {
			do {
				std::string szModule;
				for (int i = 0; i < 256; i++) {
					if ((char)modEntry.szModule[i] == '\0') break;
					szModule += (char)modEntry.szModule[i];
				}
				if (!szModule.compare(modName)) {
					CloseHandle(hSnap);
					return (uintptr_t)modEntry.modBaseAddr;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}
}



struct MyPlayer_t {
	DWORD CLocalPlayer;
	int Team;
	int Health;
	float Position[3];
	int numOfPlayers;
	DWORD ClientState;


	void ReadInformation() {
		CLocalPlayer = Read((GetModuleBaseAddress("client.dll") + dwLocalPlayer), procId);
		ClientState = Read((GetModuleBaseAddress("engine.dll") + dwClientState), procId);
		Team = Read(CLocalPlayer + dw_teamOffset, procId);
		Health = Read(CLocalPlayer + dw_healthOffset, procId);
		Position[0] = ReadFloat(CLocalPlayer + dw_pos, procId);
		Position[1] = ReadFloat(CLocalPlayer + dw_pos + 4, procId);
		Position[2] = ReadFloat(CLocalPlayer + dw_pos + 8, procId);
		numOfPlayers = 10;
	}
} myPlayer;

struct TargetList_t {
	float Distance;
	float AimbotAngle[3];

	TargetList_t() {

	}

	TargetList_t(float aimbotAngle[], float myCoords[], float enemyCoords[]) {
		Distance = Get3dDistance(myCoords[0], myCoords[1], myCoords[2], enemyCoords[0], enemyCoords[1], enemyCoords[2]);
		AimbotAngle[0] = aimbotAngle[0];
		AimbotAngle[1] = aimbotAngle[1];
		AimbotAngle[2] = aimbotAngle[2];
	}

	float Get3dDistance(float myCoordX, float myCoordY, float myCoordZ, float enCoordX, float enCoordY, float enCoordZ) {
		return sqrt(
			pow(double(myCoordX - enCoordX), 2.0)
			+ pow(double(myCoordY - enCoordY), 2.0)
			+ pow(double(myCoordZ - enCoordZ), 2.0)
		);
	}

};

struct PlayerList_t {
	DWORD CbaseEntity;
	int Team;
	int Health;
	float Position[3];
	float AimbotAngle[3];

	void ReadInformation(int player) {
		CbaseEntity = Read((GetModuleBaseAddress("client.dll") + EntityPlayer_base + (player * playerOffset)), procId);
		Team = Read(CbaseEntity + dw_teamOffset, procId);
		Health = Read(CbaseEntity + dw_healthOffset, procId);
		Position[0] = ReadFloat(CbaseEntity + dw_pos, procId);
		Position[1] = ReadFloat(CbaseEntity + dw_pos + 4, procId);
		Position[2] = ReadFloat(CbaseEntity + dw_pos + 8, procId);
	}
}PlayerList[32];


struct CompareTargetEnArray {
	bool operator()(TargetList_t& lhs, TargetList_t& rhs) {
		return lhs.Distance < rhs.Distance;
	}
};

void CalcAngle(float* src, float* dst, float* angle) {
	double delta[3] = { (src[0] - dst[0]),(src[1] - dst[1]) ,(src[2] - dst[2]) };
	double hyp = sqrt(delta[0] * delta[0] + delta[1] * delta[1]);
	angle[0] = (float)(asinf(delta[2] / hyp) * 57.295779513082f);
	angle[1] = (float)(atanf(delta[1] / delta[0]) * 57.295779513082f);
	angle[2] = 0;

	if (delta[0] >= 0.0) {
		angle[1] += 180.0f;
	}
}

void Aimbot(HANDLE gameProcess) {
	TargetList_t* TargetList = new TargetList_t[32];

	int targetLoop = 0;

	for (int i = 0; i < numOfPlayers; i++) {
		PlayerList[i].ReadInformation(i);

		if (PlayerList[i].Team == myPlayer.Team) {
			continue;
		}
		if (PlayerList[i].Health < 2) {
			continue;
		}
		CalcAngle(myPlayer.Position, PlayerList[i].Position, PlayerList[i].AimbotAngle);
		TargetList[targetLoop] = TargetList_t(PlayerList[i].AimbotAngle, myPlayer.Position, PlayerList[i].Position);

		targetLoop++;

	}

	if (targetLoop > 0) {
		std::sort(TargetList, TargetList + targetLoop, CompareTargetEnArray());

		if (TargetList[0].AimbotAngle) {
			WriteProcessMemory(gameProcess,
				(PBYTE*)(myPlayer.ClientState + dwClientState_ViewAngles),
				TargetList[0].AimbotAngle, 12, 0);
		}
	}

	targetLoop = 0;
	delete[] TargetList;
}


int __stdcall wWinMain(
	HINSTANCE instance,
	HINSTANCE previousInstance,
	PWSTR arguments,
	int commandShow)
{

	HWND GameWnd = FindWindowA(NULL, "Counter-Strike: Global Offensive - Direct3D 9");
	GetWindowThreadProcessId(GameWnd, &procId);
	HANDLE gameProcess = OpenProcess(PROCESS_ALL_ACCESS, true, procId);

	if (!procId) {
		std::cout << "Process not found" << std::endl;
		exit(1);
	}

	gui::CreateHWindow("Cheat Menu");
	gui::CreateDevice();
	gui::CreateImGui();

	//
	bool aimbot = false;
	bool bhop = false;

	
	while (gui::isRunning)
	{
		gui::BeginRender();
		gui::Render(aimbot, bhop);

		if (aimbot && !GetAsyncKeyState(RIGHT_MOUSE)) {
			myPlayer.ReadInformation();
			Aimbot(gameProcess);
		}

		gui::EndRender();

	}

	// destroy gui
	gui::DestroyImGui();
	gui::DestroyDevice();
	gui::DestroyHWindow();

	return EXIT_SUCCESS;
}
