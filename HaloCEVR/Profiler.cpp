#include "Profiler.h"

void Profiler::Init()
{
	NewFrame();
}

void Profiler::Shutdown()
{
	delete currentFrame;
	for (FrameTimings* frame : timings)
	{
		for (auto kv : frame->timings)
		{
			delete kv.second;
		}
		delete frame;
	}
	timings.clear();
}

void Profiler::NewFrame()
{
	time_point now = std::chrono::high_resolution_clock::now();
	if (currentFrame)
	{
		currentFrame->frameEnd = now;
		timings.push_back(currentFrame);
	}

	currentFrame = new FrameTimings();
	currentFrame->frameStart = std::chrono::high_resolution_clock::now();
}

int Profiler::StartEvent(const char* id)
{
	ActiveEvent& newEvent = activeEvents.emplace_back();
	newEvent.id = id;
	newEvent.startTime = std::chrono::high_resolution_clock::now();

	return activeEvents.size() - 1;
}

void Profiler::StopEvent(int eventId)
{
	time_point now = std::chrono::high_resolution_clock::now();
	
	ActiveEvent& e = activeEvents[eventId];

	float duration = std::chrono::duration<float, std::milli>(now - e.startTime).count();
	
	Timings*& timings = currentFrame->timings[e.id];

	if (!timings)
	{
		timings = new Timings();
	}

	timings->numHits++;
	timings->totalTime += duration;
	timings->minTime = (std::min)(timings->minTime, duration);
	timings->maxTime = (std::max)(timings->maxTime, duration);
}

void Profiler::GetTimings(std::vector<FrameTimings*>& outTimings) const
{
	outTimings = timings;
}

ProfilerScopeGuard::ProfilerScopeGuard(Profiler* profiler, const char* id)
{
	this->id = profiler->StartEvent(id);
	this->profiler = profiler;
}

ProfilerScopeGuard::~ProfilerScopeGuard()
{
	profiler->StopEvent(id);
}
