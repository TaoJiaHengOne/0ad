/* Copyright (C) 2025 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "CLogger.h"

#include "graphics/Canvas2D.h"
#include "graphics/FontMetrics.h"
#include "graphics/TextRenderer.h"
#include "lib/os_path.h"
#include "lib/timer.h"
#include "lib/utf8.h"
#include "ps/CConsole.h"
#include "ps/CStr.h"
#include "ps/CStrInternStatic.h"
#include "ps/Profile.h"
#include "ps/Pyrogenesis.h"

#include <ctime>
#include <fstream>
#include <iostream>
#include <utility>

#include <boost/algorithm/string/replace.hpp>

CStrW g_UniqueLogPostfix;
static const double RENDER_TIMEOUT = 10.0; // seconds before messages are deleted
static const double RENDER_TIMEOUT_RATE = 10.0; // number of timed-out messages deleted per second
static const size_t RENDER_LIMIT = 20; // maximum messages on screen at once

// Set up a default logger that throws everything away, because that's
// better than crashing. (This is particularly useful for unit tests which
// don't care about any log output.)
struct BlackHoleStreamBuf : public std::streambuf
{
} blackHoleStreamBuf;
std::ostream blackHoleStream(&blackHoleStreamBuf);
CLogger nullLogger{blackHoleStream, blackHoleStream, true};

CLogger* g_Logger = &nullLogger;

const char* html_header0 =
	"<!DOCTYPE html>\n"
	"<meta charset=\"utf-8\">\n"
	"<title>Pyrogenesis Log</title>\n"
	"<style>"
	"body { background: #eee; color: black; font-family: sans-serif; } "
	"p { background: white; margin: 3px 0 3px 0; } "
	".error { color: red; } "
	".warning { color: blue; }"
	"</style>\n"
	"<h2>0 A.D. (";

const char* html_header1 = "</h2>\n";

CLogger::CLogger(std::ostream& mainLog, std::ostream& interestingLog, const bool useDebugPrintf) :
	m_MainLog{mainLog},
	m_InterestingLog{interestingLog},
	m_UseDebugPrintf{useDebugPrintf}
{
	m_MainLog << html_header0 << engine_version << ") Main log" << html_header1;
	m_InterestingLog << html_header0 << engine_version << ") Main log (warnings and errors only)" << html_header1;
}

CLogger::~CLogger()
{
	char buffer[128];
	sprintf_s(buffer, ARRAY_SIZE(buffer), " with %d message(s), %d error(s) and %d warning(s).", m_NumberOfMessages,m_NumberOfErrors,m_NumberOfWarnings);

	time_t t = time(NULL);
	struct tm* now = localtime(&t);
	char currentDate[17];
	sprintf_s(currentDate, ARRAY_SIZE(currentDate), "%04d-%02d-%02d", 1900+now->tm_year, 1+now->tm_mon, now->tm_mday);
	char currentTime[10];
	sprintf_s(currentTime, ARRAY_SIZE(currentTime), "%02d:%02d:%02d", now->tm_hour, now->tm_min, now->tm_sec);

	//Write closing text

	m_MainLog << "<p>Engine exited successfully on " << currentDate;
	m_MainLog << " at " << currentTime << buffer << "</p>\n";

	m_InterestingLog << "<p>Engine exited successfully on " << currentDate;
	m_InterestingLog << " at " << currentTime << buffer << "</p>\n";
}

static std::string ToHTML(const char* message)
{
	std::string cmessage = message;
	boost::algorithm::replace_all(cmessage, "&", "&amp;");
	boost::algorithm::replace_all(cmessage, "<", "&lt;");
	return cmessage;
}

void CLogger::WriteMessage(const char* message, bool doRender = false)
{
	std::string cmessage = ToHTML(message);

	std::lock_guard<std::mutex> lock(m_Mutex);

	++m_NumberOfMessages;
//	if (m_UseDebugPrintf)
//		debug_printf("MESSAGE: %s\n", message);

	m_MainLog << "<p>" << cmessage << "</p>\n";
	m_MainLog.flush();

	if (doRender)
	{
		if (g_Console) g_Console->InsertMessage(std::string("INFO: ") + message);

		PushRenderMessage(Normal, message);
	}
}

void CLogger::WriteError(const char* message)
{
	std::string cmessage = ToHTML(message);

	std::lock_guard<std::mutex> lock(m_Mutex);

	++m_NumberOfErrors;
	if (m_UseDebugPrintf)
		debug_printf("ERROR: %.16000s\n", message);

	if (g_Console) g_Console->InsertMessage(std::string("ERROR: ") + message);
	m_InterestingLog << "<p class=\"error\">ERROR: " << cmessage << "</p>\n";
	m_InterestingLog.flush();

	m_MainLog << "<p class=\"error\">ERROR: " << cmessage << "</p>\n";
	m_MainLog.flush();

	PushRenderMessage(Error, message);
}

void CLogger::WriteWarning(const char* message)
{
	std::string cmessage = ToHTML(message);

	std::lock_guard<std::mutex> lock(m_Mutex);

	++m_NumberOfWarnings;
	if (m_UseDebugPrintf)
		debug_printf("WARNING: %s\n", message);

	if (g_Console) g_Console->InsertMessage(std::string("WARNING: ") + message);
	m_InterestingLog << "<p class=\"warning\">WARNING: " << cmessage << "</p>\n";
	m_InterestingLog.flush();

	m_MainLog << "<p class=\"warning\">WARNING: " << cmessage << "</p>\n";
	m_MainLog.flush();

	PushRenderMessage(Warning, message);
}

void CLogger::Render(CCanvas2D& canvas)
{
	PROFILE3("logger");

	CleanupRenderQueue();

	CStrIntern font_name{"mono-stroke-10"};
	CFontMetrics font{font_name};
	float height{font.GetHeight()};

	CTextRenderer textRenderer;
	textRenderer.SetCurrentFont(font_name);
	textRenderer.SetCurrentColor(CColor(1.0f, 1.0f, 1.0f, 1.0f));

	// Offset by an extra 35px vertically to avoid the top bar.
	textRenderer.Translate(4.0f, 35.0f + height);

	// (Lock must come after loading the CFont, since that might log error messages
	// and attempt to lock the mutex recursively which is forbidden)
	std::lock_guard<std::mutex> lock(m_Mutex);

	for (const RenderedMessage& msg : m_RenderMessages)
	{
		const char* type;
		if (msg.method == Normal)
		{
			type = "info";
			textRenderer.SetCurrentColor(CColor(0.0f, 0.8f, 0.0f, 1.0f));
		}
		else if (msg.method == Warning)
		{
			type = "warning";
			textRenderer.SetCurrentColor(CColor(1.0f, 1.0f, 0.0f, 1.0f));
		}
		else
		{
			type = "error";
			textRenderer.SetCurrentColor(CColor(1.0f, 0.0f, 0.0f, 1.0f));
		}

		const CVector2D savedTranslate = textRenderer.GetTranslate();

		textRenderer.PrintfAdvance(L"[%8.3f] %hs: ", msg.time, type);
		// Display the actual message in white so it's more readable
		textRenderer.SetCurrentColor(CColor(1.0f, 1.0f, 1.0f, 1.0f));
		textRenderer.Put(0.0f, 0.0f, msg.message.c_str());

		textRenderer.ResetTranslate(savedTranslate);

		textRenderer.Translate(0.0f, height);
	}

	canvas.DrawText(textRenderer);
}

void CLogger::PushRenderMessage(ELogMethod method, const char* message)
{
	double now = timer_Time();

	// Add each message line separately
	const char* pos = message;
	const char* eol;
	while ((eol = strchr(pos, '\n')) != NULL)
	{
		if (eol != pos)
		{
			RenderedMessage r = { method, now, std::string(pos, eol) };
			m_RenderMessages.push_back(r);
		}
		pos = eol + 1;
	}
	// Add the last line, if we didn't end on a \n
	if (*pos != '\0')
	{
		RenderedMessage r = { method, now, std::string(pos) };
		m_RenderMessages.push_back(r);
	}
}

void CLogger::CleanupRenderQueue()
{
	std::lock_guard<std::mutex> lock(m_Mutex);

	if (m_RenderMessages.empty())
		return;

	double now = timer_Time();

	// Initialise the timer on the first call (since we can't do it in the ctor)
	if (m_RenderLastEraseTime == -1.0)
		m_RenderLastEraseTime = now;

	// Delete old messages, approximately at the given rate limit (and at most one per frame)
	if (now - m_RenderLastEraseTime > 1.0/RENDER_TIMEOUT_RATE)
	{
		if (m_RenderMessages[0].time + RENDER_TIMEOUT < now)
		{
			m_RenderMessages.pop_front();
			m_RenderLastEraseTime = now;
		}
	}

	// If there's still too many then delete the oldest
	if (m_RenderMessages.size() > RENDER_LIMIT)
		m_RenderMessages.erase(m_RenderMessages.begin(), m_RenderMessages.end() - RENDER_LIMIT);
}

CLogger::ScopedReplacement::ScopedReplacement(std::ostream& mainLog, std::ostream& interestingLog,
	const bool useDebugPrintf) :
	m_ThisLogger{mainLog, interestingLog, useDebugPrintf},
	m_OldLogger{std::exchange(g_Logger, &m_ThisLogger)}
{}

CLogger::ScopedReplacement::~ScopedReplacement()
{
	g_Logger = m_OldLogger;
}

namespace
{
std::ofstream OpenLogFile(const wchar_t* filePrefix, const char* logName)
{
	OsPath path{psLogDir() / (filePrefix + g_UniqueLogPostfix + L".html")};
	debug_printf("FILES| %s written to '%s'\n", logName, path.string8().c_str());
	return std::ofstream{OsString(path), std::ofstream::trunc};
}
}

FileLogger::FileLogger() :
	m_MainLog{OpenLogFile(L"mainlog", "Main log")},
	m_InterestingLog{OpenLogFile(L"interestinglog", "Interesting log")},
	m_ScopedReplacement{m_MainLog, m_InterestingLog, true}
{}

TestLogger::TestLogger() :
	m_ScopedReplacement{m_Stream, blackHoleStream, false}
{}

std::string TestLogger::GetOutput()
{
	return m_Stream.str();
}

TestStdoutLogger::TestStdoutLogger() :
	m_ScopedReplacement{std::cout, blackHoleStream, false}
{}
