#include "DiagnosticLogger.h"
#include "Logger.h"

DiagnosticLogger& DiagnosticLogger::Get()
{
	static DiagnosticLogger instance;
	return instance;
}

void DiagnosticLogger::Start(const std::string& filename)
{
	if (bActive)
	{
		Stop();
	}

	file.open(filename, std::ios::out | std::ios::trunc);
	if (!file)
	{
		Logger::log << "[Diagnostics] Failed to open " << filename << std::endl;
		return;
	}

	bActive = true;
	frameNumber = 0;
	subFrameCounter = 0;
	framesSinceTransition = 999999;
	startTime = std::chrono::high_resolution_clock::now();

	WriteHeader();
	Logger::log << "[Diagnostics] CSV logging started -> " << filename << std::endl;
}

void DiagnosticLogger::Stop()
{
	if (!bActive)
	{
		return;
	}

	bActive = false;
	file.flush();
	file.close();
	Logger::log << "[Diagnostics] CSV logging stopped (" << frameNumber << " frames)" << std::endl;
}

void DiagnosticLogger::WriteHeader()
{
	file << "Frame,SubFrame,TimestampMs,Source,"
		<< "PlayerX,PlayerY,PlayerZ,"
		<< "VelX,VelY,VelZ,"
		<< "CamX,CamY,CamZ,"
		<< "CamLookX,CamLookY,CamLookZ,"
		<< "FrustumX,FrustumY,FrustumZ,"
		<< "FrustumFaceX,FrustumFaceY,FrustumFaceZ,"
		<< "HmdX,HmdY,HmdZ,"
		<< "LocOffsetX,LocOffsetY,LocOffsetZ,"
		<< "InVehicle,IgnoreRoomScale,YawOffset,DeltaTime,"
		<< "Extra\n";
}

void DiagnosticLogger::BeginFrame(bool bInVehicle)
{
	if (!bActive)
	{
		return;
	}

	frameNumber++;
	subFrameCounter = 0;

	// Detect vehicle transitions
	if (bInVehicle != lastVehicleState)
	{
		framesSinceTransition = 0;
	}
	else if (framesSinceTransition < 999999)
	{
		framesSinceTransition++;
	}

	lastVehicleState = bInVehicle;
}

void DiagnosticLogger::LogRow(
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
)
{
	if (!bActive)
	{
		return;
	}

	// Windowed logging: skip if we're far from a vehicle transition
	if (windowFrames > 0 && framesSinceTransition > windowFrames)
	{
		return;
	}

	auto now = std::chrono::high_resolution_clock::now();
	double ms = std::chrono::duration<double, std::milli>(now - startTime).count();

	char buf[1024];
	std::snprintf(buf, sizeof(buf),
		"%d,%d,%.3f,%s,"
		"%.4f,%.4f,%.4f,"
		"%.4f,%.4f,%.4f,"
		"%.4f,%.4f,%.4f,"
		"%.6f,%.6f,%.6f,"
		"%.4f,%.4f,%.4f,"
		"%.6f,%.6f,%.6f,"
		"%.4f,%.4f,%.4f,"
		"%.4f,%.4f,%.4f,"
		"%d,%d,%.4f,%.6f,"
		"%s\n",
		frameNumber, subFrameCounter, ms, source,
		playerX, playerY, playerZ,
		velX, velY, velZ,
		camX, camY, camZ,
		camLookX, camLookY, camLookZ,
		frustumX, frustumY, frustumZ,
		frustumFaceX, frustumFaceY, frustumFaceZ,
		hmdX, hmdY, hmdZ,
		locOffsetX, locOffsetY, locOffsetZ,
		bInVehicle ? 1 : 0, bIgnoreRoomScale ? 1 : 0,
		yawOffset, deltaTime,
		extra
	);

	file << buf;
	subFrameCounter++;
}
