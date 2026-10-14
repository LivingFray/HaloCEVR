#pragma once
namespace CrashDiagnostics {
// Logs first-chance fatal exception context without handling/suppressing it.
bool Start();
void Stop();
}
