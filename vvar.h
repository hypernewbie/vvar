/*
	===========================================================================
	Copyright (C) 1999-2005 Id Software, Inc.

	This file was heavily modified from Quake III Arena source code.
	The following license applies only to this file and its heavy modifications.

	Quake III Arena source code is free software; you can redistribute it
	and/or modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of the License,
	or (at your option) any later version.

	Quake III Arena source code is distributed in the hope that it will be
	useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Quake III Arena source code; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
	===========================================================================
*/

#pragma once
#include <map>
#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <unordered_map>

// Macro to disallow copy constructor and assignment operator
#define DISALLOW_COPY(className) \
    className(const className&) = delete; \
    className& operator=(const className&) = delete;

typedef std::vector< uint8_t >  veFileData;

// ------------------------------------ Utils ----------------------------------------

// ref: ioq3 source code
inline int veIsPrint( int c )
{
	if ( c >= 0x20 && c <= 0x7E )
		return ( 1 );
	return ( 0 );
}

// ref: ioq3 source code
inline int veIsLower( int c )
{
	if (c >= 'a' && c <= 'z')
		return ( 1 );
	return ( 0 );
}

// ref: ioq3 source code
inline int veIsUpper( int c )
{
	if (c >= 'A' && c <= 'Z')
		return ( 1 );
	return ( 0 );
}

// ref: ioq3 source code
inline int veIsAlpha( int c )
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return ( 1 );
	return ( 0 );
}

// ref: ioq3 source code
inline bool veIsANumber( const char *s )
{
	char *p;
	double d;

	if( *s == '\0' )
		return false;

	d = strtod( s, &p );

	return *p == '\0';
}

// ref: ioq3 source code
inline bool veIsIntegral( float f )
{
	return (int)f == f;
}

// ------------------------------------ Key / value info strings ----------------------------------------

class veIVar
{
	DISALLOW_COPY( veIVar )

	static std::unordered_map< std::string, std::unordered_map< std::string, std::string > > m_globalIVarTable;

public:
	static const char* get( const char *s, const char *key );
	static void remove( const char *s, const char *key );
	static void set( const char *s, const char *key, const char *value );
};

// ------------------------------------ Console Variables Definition ----------------------------------------

// ref: ioq3 source code

// Set to cause it to be saved to vars.rc
// used for system variables, not for player
// specific configurations
#define	VE_CVAR_ARCHIVE 0x0001

#define	VE_CVAR_USERINFO 0x0002	// sent to server on connect or change
#define	VE_CVAR_SERVERINFO 0x0004	// sent in response to front end requests
#define	VE_CVAR_SYSTEMINFO 0x0008	// these cvars will be duplicated on all clients

// Don't allow change from console at all,
// but can be set from the command line
#define	VE_CVAR_INIT 0x0010	

// Will only change when C code next does
// a Cvar_Get(), so it can't be changed
// without proper initialization.  modified
// will be set, even though the value hasn't
// changed yet
#define	VE_CVAR_LATCH 0x0020	

// Display only, cannot be set by user at all
#define	VE_CVAR_ROM 0x0040	

// Can not be changed if cheats are disabled
#define VE_CVAR_CHEAT 0x0200

// Do not clear when a cvar_restart is issued
#define VE_CVAR_NORESTART 0x0400

// Prevent modifying this var from VMs or the server
#define VE_CVAR_PROTECTED 0x2000	

// Use this to allow directly setting of integer values, which will be propagated per-frame to
// the string representation.
#define VE_CVAR_ALLOW_SET_INTEGER 0x4000	

// These flags are only returned by the Cvar_Flags() function
#define VE_CVAR_MODIFIED 0x40000000	// Cvar was modified
#define VE_CVAR_NONEXISTENT 0x80000000	// Cvar doesn't exist.

#define VE_MAX_CVAR_VALUE_STRING 4096

//  veCVar variables are used to hold scalar or string variables that can be changed
//  or displayed at the console or prog code as well as accessed directly
//  in C code.
//
//  The user can access cvars from the console in three ways:
//  r_draworder			prints the current value
//  r_draworder 0		sets the current value to 0
//  set r_draworder 0	as above, but creates the cvar if not present
//
//  Cvars are restricted from having the same names as commands to keep this
//  interface from being ambiguous.
//
//  The are also occasionally used to communicated information between different
//  modules of the program.
//
// ref: ioq3 source code
//
class veCVar
{
	DISALLOW_COPY( veCVar )

	std::string m_name;
	std::string m_string;
	std::string m_resetString;		// cvar_restart will reset to this value
	std::string m_latchedString;	// for CVAR_LATCH vars

	int   m_flags = 0;
	bool  m_modified = false;		// set each time the cvar is changed
	int   m_modificationCount = 0;	// incremented each time the cvar is changed
	float m_value = 0.0f;			// atof( string )
	int   m_integer = 0;			// atoi( string )
	bool  m_validate = false;
	bool  m_integral = false;
	float m_min = 0.0f;
	float m_max = 0.0f;
	std::string m_description;

	// Singleton members.
	static std::map< std::string, std::unique_ptr< veCVar > > m_globalCVarTable;
	static int m_modifiedFlags;

protected:
	static bool validateString( const char *s );
	static const char* validate( veCVar *var, const char *value, bool warn );

public:
	veCVar();
	virtual ~veCVar();

	inline auto getModified()
	{
		return m_modified;
	}

	inline auto getModificationCount()
	{
		return m_modified;
	}

	inline auto& getValue()
	{
		return m_value;
	}

	inline auto& getInteger()
	{
		return m_integer;
	}

	inline bool& getBool()
	{
		return *( reinterpret_cast< bool* >( &m_integer ) );
	}

	inline auto& getString()
	{
		return m_string;
	}

	inline auto& getName()
	{
		return m_name;
	}

	inline auto& getDescription()
	{
		return m_description;
	}

	inline auto& getFlags()
	{
		return m_flags;
	}

	static inline auto& getModifiedFlags()
	{
		return m_modifiedFlags;
	}


	// Creates the variable if it doesn't exist, or returns the existing one
	// if it exists, the value will not be changed, but flags will be ORed in
	// that allows variables to be unarchived without needing bitflags
	// if value is "", the value will not override a previously set value.
	//
	static veCVar* get( const char* varName, const char* value, int flags );

	// Will create the variable with no flags if it doesn't exist
	static void set( const char* varName, const char* value );

	// Same as veCVar::set, but allows more control over setting of cvar
	static veCVar* set2( const char* varName, const char* value, bool force );

	// Sometimes we set variables from an untrusted source: fail if flags & CVAR_PROTECTED
	static void setSafe( const char* varName, const char* value );

	// Don't set the cvar immediately
	static void setLatched( const char* varName, const char* value );

	// Expands value to a string and calls Cvar_Set/Cvar_SetSafe
	static void	setValue( const char *varName, float value );
	static void	setValueSafe( const char *varName, float value );

	// Returns 0 if not defined or non numeric
	static float variableValue( const char *varName );
	static int variableIntegerValue( const char *varName );

	// Returns an empty string if not defined
	static const char* variableString( const char *varName );
	static void	variableStringBuffer( const char *varName, char *buffer, int bufsize );

	// Returns CVAR_NONEXISTENT if cvar doesn't exist or the flags of that particular CVAR.
	static int	flags( const char *varName );

	// Callback with each valid string
	static void	commandCompletion( std::function< void( const char *s ) > callback );

	static void reset( const char *varName );
	static void forceReset( const char *varName );

	// Reset all testing vars to a safe value
	static void	setCheatState( void );

	// Called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
	// command.  Returns true if the command was a variable reference that
	// was handled. (print or change)
	static bool command( void );

	// Writes lines containing "set variable value" for all variables
	// with the archive flag set to true.
	static void writeVariables( veFileData& f );

	static void	init( void );

	static void checkRange( veCVar *var, float minVal, float maxVal, bool shouldBeIntegral );
	static void setDescription( veCVar *var, const char *description );

	static void	restart();

	static veCVar* find( const char *varName );
	static void print( veCVar *v );
	static void list( const char* match = nullptr );
	static void listModified( const char* match = nullptr );

	static void remove( const char* varName );
	static void updateFromIntegerFloatValues();
};

extern veCVar* sv_cheats;

// ------------------------------------------------- Console Commands ------------------------------------------------------------

#define	VE_MAX_STRING_TOKENS 1024
#define	VE_MAX_TOKEN_LENGTH 4096

// Command execution takes a null terminated string, breaks it into tokens,
// then searches for a command or variable that matches the first token.
// ref: ioq3 source code

typedef std::function< void(void) > veCmdFunc;

// Parameters for command buffer stuffing
// ref: ioq3 source code
enum veCmdExecWhen
{
	VE_CMD_EXEC_NOW,		// Don't return until completed.
	VE_CMD_EXEC_INSERT,		// Insert at current position, but don't run yet
	VE_CMD_EXEC_APPEND		// Add to end of the command buffer (normal case)
};

struct veCmdFuncHandler {
	std::string name;
	veCmdFunc func = nullptr;
	std::string complete;
};

class veCmd
{
	DISALLOW_COPY( veCmd )

	// Current execution state.
	int m_wait = 0;
	std::string m_text;
	std::string m_line;

	// Command tokenisation state.
	std::vector< int > m_argv; // Points into m_tokens
	std::vector< char > m_tokens; // Will have 0 bytes inserted
	std::string m_cmd; // The original command we received (no token processing).

	// Possible commands to execute
	std::unordered_map< std::string, veCmdFuncHandler > m_functions;

protected:
	void execute();
	void tokenizeString2( const char *text, bool ignoreQuotes );

public:
	veCmd();
	virtual ~veCmd();

	// Called by the init functions of other parts of the program to
	// register commands and functions to call for them.
	// The cmd_name is referenced later, so it should not be in temp memory
	// if function is NULL, the command will be forwarded to the server
	// as a clc_clientCommand instead of executed locally.
	//
	void addCommand( const char *name, veCmdFunc function );

	void removeCommand( const char *name );

	void commandCompletion( std::function< void( const char *s, const char *expr ) > callback );

	// Callback with each valid string
	void setCommandCompletion( const char *command, const std::string& complete );

	// The functions that execute commands get their parameters with these
	// functions. argv ) will return an empty string, not a NULL
	// if arg > argc, so string operations are allways safe.
	//
	int argc( void );
	const char* argv( int arg );
	std::string args( void );
	std::string argsFrom( int arg );
	const char* cmd( void );


	// Takes a null terminated string.  Does not need to be /n terminated.
	// breaks the string up into arg tokens.
	void	tokenizeString( const char *text );
	void	tokenizeStringIgnoreQuotes( const char *text );
	
	// Parses a single line of text into arguments and tries to execute it
	// as if it was typed at the console
	void executeString( const char *text );
	void executeTokenized();
	void execute( veCmdExecWhen when, const char* text = nullptr );

	static const char* stringContains( const char *str1, const char *str2, int caseSensitive = false );
	static int filter( const char *filter, const char *name, int caseSensitive = false );

	//------------------- Setters / Getters -------------------

	inline auto& getFunctionsList()
	{
		return m_functions;
	}

	inline void setWait( int wait )
	{
		m_wait = wait;
	}

	inline auto getWait()
	{
		return m_wait;
	}

	inline int getArgvIdx( int arg )
	{
		if ( arg < 0 || arg >= m_argv.size() ) return -1;
		return m_argv[ arg ];
	}

	inline int getArgvLen( int arg )
	{
		auto s = this->argv( arg );
		return s ? (int) strlen( s ) : 0;
	}
};

// Global console command singleton.
extern std::unique_ptr< veCmd > g_cmd;
inline veCmd& veGetCmd()
{
	if ( !g_cmd.get() )
		g_cmd = std::make_unique< veCmd >();
	return *g_cmd.get();
}

void veCmd_InitDefaultFunctions();


// ------------------------------------------------- Misc. Other Utils -------------------------------------------------

// Ignore __attribute__ on non-gcc platforms
#if !defined(__GNUC__) && !defined(__llvm__)
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

char* veq3_va( const char *format, ... ) __attribute__( ( format( printf, 1, 2 ) ) );