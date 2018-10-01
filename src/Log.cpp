#include <winsock2.h>
#include <mutex>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "Log.hpp"

static std::mutex lock;
static bool initialised = false;
static FILE *log_fh = NULL;
static bool trace_enabled = false;

static void _log_init()
{
	if(initialised)
	{
		return;
	}
	
	const char *log_name = getenv("DPLITE_LOG");
	if(log_name != NULL)
	{
		log_fh = fopen("a", log_name);
	}
	
	const char *t = getenv("DPLITE_TRACE");
	trace_enabled = (t && atoi(t) != 0);
	
	initialised = true;
}

static void _log_fini()
{
	initialised = false;
	
	trace_enabled = false;
	
	if(log_fh != NULL)
	{
		fclose(log_fh);
		log_fh = NULL;
	}
}

void log_init()
{
	std::unique_lock<std::mutex> l(lock);
	_log_init();
}

void log_fini()
{
	std::unique_lock<std::mutex> l(lock);
	_log_fini();
}

bool log_trace_enabled()
{
	std::unique_lock<std::mutex> l(lock);
	_log_init();
	
	return trace_enabled;
}

void log_printf(const char *fmt, ...)
{
	std::unique_lock<std::mutex> l(lock);
	_log_init();
	
	if(log_fh != NULL)
	{
		fprintf(log_fh, "[thread=%u time=%u] ",
			(unsigned)(GetCurrentThreadId()), (unsigned)(GetTickCount()));
		
		va_list argv;
		va_start(argv, fmt);
		vfprintf(log_fh, fmt, argv);
		va_end(argv);
		
		fprintf(log_fh, "\n");
	}
}
