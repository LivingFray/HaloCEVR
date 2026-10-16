#pragma once
#include <fstream>
#include <string>
#include <chrono>
#include <cstdio>

class DiagnosticLogger {
public:
	static DiagnosticLogger& Get();

	void Start(const std::string& filename = "VR/diagnostics.csv");
	void Stop();

	bool IsActive() const { return bActive; }

	// Call once per render frame from PreDrawFrame to track frame count and vehicle transitions
	void BeginFrame(bool bInVehicle);

	// Set how many frames around a vehicle transition to log (0 = always log)
	void SetWindowSize(int frames) { windowFrames = frames; }

	// Write a single CSV row of diagnostic data
	void LogRow(
		const char* source,
		float playerX, float playerY, float playerZ,
		float velX, float velY, float velZ,
		float camX, float camY, float camZ,
		float camLookX, float camLookY, float camLookZ,
		float frustumX, float frustumY, float frustumZ,
		float frustumFaceX, float frustumFaceY, float frustumFaceZ,
		float hmdX, float hmdY, float hmdZ,
		float locOffsetX, float locOffsetY, float locOffsetZ,
		bool bInVehicle,
		bool bIgnoreRoomScale,
		float yawOffset,
		float deltaTime,
		const char* extra
	);

private:
	DiagnosticLogger() = default;

	std::ofstream file;
	bool bActive = false;

	int frameNumber = 0;
	int subFrameCounter = 0;

	// Windowed logging around vehicle transitions
	int windowFrames = 120;
	int framesSinceTransition = 999999;
	bool lastVehicleState = false;

	std::chrono::high_resolution_clock::time_point startTime;

	void WriteHeader();
};
