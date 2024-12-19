#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>

#define USE_PROFILER 0

#if USE_PROFILER
#define VR_PROFILE_SCOPE(x) ProfilerScopeGuard profiler_##x(&Game::instance.profiler, #x);
#define VR_PROFILE_START(x) int profiler_##x = Game::instance.profiler.StartEvent(#x);
#define VR_PROFILE_STOP(x) Game::instance.profiler.StopEvent(profiler_##x);
#else
#define VR_PROFILE_SCOPE(x)
#define VR_PROFILE_START(x)
#define VR_PROFILE_STOP(x)
#endif

class ProfilerScopeGuard
{
public:
	ProfilerScopeGuard() = delete;
	ProfilerScopeGuard(class Profiler* profiler, const char* id);
	~ProfilerScopeGuard();

protected:
	int id;
	class Profiler* profiler;
};

class Profiler
{
public:
	typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;

	struct Timings
	{
		int numHits = 0;
		float minTime = FLT_MAX;
		float maxTime = -FLT_MAX;
		float totalTime = 0.0f;
	};

	struct FrameTimings
	{
		time_point frameStart;
		time_point frameEnd;
		std::unordered_map<std::string, Timings*> timings;
	};


	void Init();
	void Shutdown();

	void NewFrame();

	int StartEvent(const char* id);
	void StopEvent(int eventId);

	void GetTimings(std::vector<FrameTimings*>& outTimings) const;

protected:

	std::vector<FrameTimings*> timings;
	FrameTimings* currentFrame = nullptr;

	struct ActiveEvent
	{
		time_point startTime;
		std::string id;
	};

	std::vector<ActiveEvent> activeEvents;
};

