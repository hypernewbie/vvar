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

#include "vvar.h"

#include <array>
#include <fstream>

veCVar* sv_cheats = nullptr;
std::unique_ptr< veCmd > g_cmd;

// Field completion state.
static std::string g_fieldCompletionString;
static std::string g_fieldShortestMatch;
static int g_fieldMatchCount;

static bool veInfoStringValueIsValid( const std::string& value )
{
	return value.find( '\\' ) == std::string::npos
		&& value.find( '"' ) == std::string::npos
		&& value.find( ';' ) == std::string::npos;
}

static bool veIsValidConsoleToken( const char* value )
{
	return value && *value
		&& strchr( value, '\\' ) == nullptr
		&& strchr( value, '"' ) == nullptr
		&& strchr( value, ';' ) == nullptr;
}

static void veCopyString( char* buffer, int bufsize, const char* value )
{
	if( !buffer || bufsize <= 0 ) {
		return;
	}

	if( !value ) {
		buffer[0] = '\0';
		return;
	}

	std::snprintf( buffer, static_cast< size_t >( bufsize ), "%s", value );
}

// ------------------------------------ Key / value info strings ----------------------------------------

// ref: ioq3 source code

std::unordered_map< std::string, std::unordered_map< std::string, std::string > > veIVar::m_globalIVarTable;
thread_local std::string veIVar::m_serializedInfoString;

const char* veIVar::get( const char *s, const char *key )
{
	auto itr = m_globalIVarTable.find( s );
	if ( itr == m_globalIVarTable.end() )
		return nullptr;

	auto itr2 = itr->second.find( key );
	if ( itr2 == itr->second.end() )
		return nullptr;

	return itr2->second.c_str();
}

void veIVar::remove( const char *s, const char *key )
{
	auto itr = m_globalIVarTable.find( s );
	if ( itr == m_globalIVarTable.end() )
		return;

	auto itr2 = itr->second.find( key );
	if ( itr2 == itr->second.end() )
		return;

	itr->second.erase( itr2 );
	if( itr->second.empty() ) {
		m_globalIVarTable.erase( itr );
	}
}

void veIVar::set( const char *s, const char *key, const char *value )
{
	m_globalIVarTable[s][key] = value;
}

const char* veIVar::toString( const char *s )
{
	m_serializedInfoString.clear();

	auto itr = m_globalIVarTable.find( s ? s : "" );
	if( itr == m_globalIVarTable.end() ) {
		return "";
	}

	for( const auto& kv : itr->second ) {
		if( !veInfoStringValueIsValid( kv.first ) || !veInfoStringValueIsValid( kv.second ) ) {
			continue;
		}

		m_serializedInfoString += '\\';
		m_serializedInfoString += kv.first;
		m_serializedInfoString += '\\';
		m_serializedInfoString += kv.second;
	}

	return m_serializedInfoString.c_str();
}

void veIVar::fromString( const char *s, const char *infoString )
{
	auto& section = m_globalIVarTable[s ? s : ""];
	section.clear();

	if( !infoString || !*infoString ) {
		return;
	}

	auto cursor = infoString;
	while( *cursor ) {
		if( *cursor == '\\' ) {
			cursor++;
		}
		if( !*cursor ) {
			break;
		}

		const char* keyStart = cursor;
		while( *cursor && *cursor != '\\' ) {
			cursor++;
		}
		std::string key( keyStart, cursor - keyStart );
		if( !*cursor ) {
			break;
		}

		cursor++;
		const char* valueStart = cursor;
		while( *cursor && *cursor != '\\' ) {
			cursor++;
		}
		std::string value( valueStart, cursor - valueStart );

		if( !key.empty() && veInfoStringValueIsValid( key ) && veInfoStringValueIsValid( value ) ) {
			section[key] = value;
		}
	}

	if( section.empty() ) {
		m_globalIVarTable.erase( s ? s : "" );
	}
}

// ------------------------------------ Console Variables Implementation ----------------------------------------

// ref: ioq3 source code

std::map< std::string, std::unique_ptr< veCVar > > veCVar::m_globalCVarTable;
int veCVar::m_modifiedFlags = 0;

static void veCVarCmd_PrintVarFlags( veCVar* var )
{
	if( var->getFlags() & VE_CVAR_SERVERINFO ) {
		dinfo( "S" );
	} else {
		dinfo( " " );
	}
	if( var->getFlags() & VE_CVAR_SYSTEMINFO ) {
		dinfo( "s" );
	} else {
		dinfo( " " );
	}
	if( var->getFlags() & VE_CVAR_USERINFO ) {
		dinfo( "U" );
	} else {
		dinfo( " " );
	}
	if( var->getFlags() & VE_CVAR_ROM ) {
		dinfo( "R" );
	} else {
		dinfo( " " );
	}
	if( var->getFlags() & VE_CVAR_INIT ) {
		dinfo( "I" );
	} else {
		dinfo( " " );
	}
	if( var->getFlags() & VE_CVAR_ARCHIVE ) {
		dinfo( "A" );
	} else {
		dinfo( " " );
	}
	if( var->getFlags() & VE_CVAR_LATCH ) {
		dinfo( "L" );
	} else {
		dinfo( " " );
	}
	if( var->getFlags() & VE_CVAR_CHEAT ) {
		dinfo( "C" );
	} else {
		dinfo( " " );
	}
}

veCVar::veCVar()
{

}

veCVar::~veCVar()
{
}

bool veCVar::validateString( const char *s )
{
	if( !s ) {
		return false;
	}
	if( strchr( s, '\\' ) ) {
		return false;
	}
	if( strchr( s, '\"' ) ) {
		return false;
	}
	if( strchr( s, ';' ) ) {
		return false;
	}
	return true;
}

const char* veCVar::validate( veCVar *var, const char *value, bool warn )
{
	static char s[VE_MAX_CVAR_VALUE_STRING];
	float valuef;
	bool changed = false;

	if( !var->m_validate )
		return value;

	if( !value )
		return value;

	if( veIsANumber( value ) ) {
		valuef = (float) atof( value );

		if( var->m_integral ) {
			if( !veIsIntegral( valuef ) ) {
				if( warn )
					derr( "WARNING: cvar '%s' must be integral", var->m_name.c_str() );

				valuef = ( float ) ( ( int ) valuef );
				changed = true;
			}
		}
	} else {
		if( warn )
			derr( "WARNING: cvar '%s' must be numeric", var->m_name.c_str() );

		valuef = (float) atof( var->m_resetString.c_str() );
		changed = true;
	}

	if( valuef < var->m_min ) {
		if( warn ) {
			if( changed )
				derr( " and is" );
			else
				derr( "WARNING: cvar '%s'", var->m_name.c_str() );

			if( veIsIntegral( var->m_min ) )
				derr( " out of range (min %d)", ( int ) var->m_min );
			else
				derr( " out of range (min %f)", var->m_min );
		}

		valuef = var->m_min;
		changed = true;
	} else if( valuef > var->m_max ) {
		if( warn ) {
			if( changed )
				derr( " and is" );
			else
				derr( "WARNING: cvar '%s'", var->m_name.c_str() );

			if( veIsIntegral( var->m_max ) )
				derr( " out of range (max %d)", ( int ) var->m_max );
			else
				derr( " out of range (max %f)", var->m_max );
		}

		valuef = var->m_max;
		changed = true;
	}

	if( changed ) {
		if( veIsIntegral( valuef ) ) {
			std::snprintf( s, sizeof( s ), "%d", ( int ) valuef );

			if( warn )
				derr( ", setting to %d\n", ( int ) valuef );
		} else {
			std::snprintf( s, sizeof( s ), "%f", valuef );

			if( warn )
				derr( ", setting to %f\n", valuef );
		}

		return s;
	} else {
		return value;
	}
}

veCVar* veCVar::find( const char *varName )
{
	auto itr = m_globalCVarTable.find( varName );
	if( itr == m_globalCVarTable.end() ) {
		return nullptr;
	}
	return itr->second.get();
}


veCVar* veCVar::get( const char* varName, const char* value, int flags )
{
	if ( !varName || ! value ) {
		derr( "veCVar::get() NULL parameter!\n" );
		return nullptr;
	}

	bool invalidName = false;
	if( !veCVar::validateString( varName ) ) {
		derr( "invalid cvar name string: %s\n", varName );
		varName = "BADNAME";
		invalidName = true;
	}

	auto var = veCVar::find( varName );
	if( var ) {
		value = veCVar::validate( var, value, false );

		var->m_flags |= flags;

		// Only allow one non-empty reset string without a warning
		if( !var->m_resetString.length() ) {
			// We don't have a reset string yet
			var->m_resetString = value;
		} else if( value[0] && var->m_resetString != value ) {
			derr( "veCVar::get() Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n", varName, var->m_resetString.c_str(), value );
		}

		// If we have a latched string, take that value now
		if( var->m_latchedString.length() > 0 ) {
			auto s = var->m_latchedString;
			var->m_latchedString.clear();
			veCVar::set2( varName, s.c_str(), true );
		}

		// ZOID--needs to be set so that cvars the game sets as 
		// SERVERINFO get sent to clients
		veCVar::m_modifiedFlags |= flags;

		return var;
	}

	//
	// Allocate a new cvar
	//

	m_globalCVarTable[varName] = std::unique_ptr< veCVar >( new veCVar() );
	var = m_globalCVarTable[varName].get();
	if( !var )
		derr_fatal( "veCVar::get() Failed to allocate new cvar." );

	var->m_name = varName;
	var->m_string = invalidName ? "BADNAME" : value;
	var->m_modified = false;
	var->m_modificationCount = 0;
	var->m_value = (float) atof( var->m_string.c_str() );
	var->m_integer = (int) atoi( var->m_string.c_str() );
	var->m_resetString = value;
	var->m_validate = false;
	var->m_description.clear();

	var->m_flags = flags;
	// Note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	veCVar::m_modifiedFlags |= var->m_flags;

	return var;
}

void veCVar::print( veCVar *v )
{
	dinfo( "\"%s\" is:\"%s\"", v->m_name.c_str(), v->m_string.c_str() );

	if( !( v->m_flags & VE_CVAR_ROM ) ) {
		if( v->m_string == v->m_resetString ) {
			dinfo( ", the default" );
		} else {
			dinfo( " default:\"%s\"", v->m_resetString.c_str() );
		}
	}
	dinfo( "\n" );

	if( v->m_latchedString.length() ) {
		dinfo( "latched: \"%s\"\n", v->m_latchedString.c_str() );
	}

	if( v->m_description.length() ) {
		dinfo( "%s\n", v->m_description.c_str() );
	}
}

void veCVar::list( const char* match )
{
	for( auto& cv : m_globalCVarTable ) {
		auto var = cv.second.get();
		if( !var || !var->m_name.length() || ( match && !veCmd::filter( match, var->m_name.c_str(), false ) ) )
			continue;
		veCVarCmd_PrintVarFlags( var );
		dinfo(" %s \"%s\"\n", var->m_name.c_str(), var->m_string.c_str());
	}
	dinfo ("\n%i total cvars\n", m_globalCVarTable.size());
}

void veCVar::listModified( const char* match )
{
	int totalModified = 0;
	for( auto& cv : m_globalCVarTable ) {
		auto var = cv.second.get();
		if( !var || !var->m_name.length() || !var->m_modificationCount )
			continue;

		auto& value = var->m_latchedString.length() ? var->m_latchedString : var->m_string;
		if( value == var->m_resetString )
			continue;

		totalModified++;

		if( match && !veCmd::filter( match, var->m_name.c_str(), false ) )
			continue;

		veCVarCmd_PrintVarFlags( var );
		dinfo( " %s \"%s\", default \"%s\"\n", var->m_name.c_str(), value.c_str(), var->m_resetString.c_str() );
	}

	dinfo( "\n%i total modified cvars\n", totalModified );
}

void veCVar::set( const char* varName, const char* value )
{
	veCVar::set2( varName, value, true );
}

veCVar* veCVar::set2( const char* varName, const char* value, bool force )
{
	if ( !veCVar::validateString( varName ) ) {
		dinfo("invalid cvar name string: %s\n", varName );
		varName = "BADNAME";
	}

	auto var = veCVar::find( varName );
	if( !var ) {
		if( !value ) {
			return nullptr;
		}

		// Create it
		return veCVar::get( varName, value, 0 );
	}

	if( !value ) {
		value = var->m_resetString.c_str();
	}

	value = veCVar::validate( var, value, true );

	if( var->m_string == value )
		return var;

	// Note what types of cvars have been modified (userinfo, archive, serverinfo, systeminfo)
	m_modifiedFlags |= var->m_flags;

	if( !force ) {
		if( var->m_flags & VE_CVAR_ROM ) {
			dinfo( "%s is read only.\n", varName );
			return var;
		}

		if( var->m_flags & VE_CVAR_INIT ) {
			dinfo( "%s is write protected.\n", varName );
			return var;
		}

		if( ( var->m_flags & VE_CVAR_CHEAT ) && !sv_cheats->getInteger() ) {
			dinfo( "%s is cheat protected.\n", varName );
			return var;
		}

		if( var->m_flags & VE_CVAR_LATCH ) {
			if( var->m_latchedString.length() ) {
				if( var->m_latchedString == value )
					return var;
				var->m_latchedString.clear();
			} else {
				if( var->m_string == value )
					return var;
			}

			dinfo( "%s will be changed upon restarting.\n", varName );
			var->m_latchedString = value;
			var->m_modified = true;
			var->m_modificationCount++;
			return var;
		}
	} else {
		if( var->m_latchedString.length() ) {
			var->m_latchedString.clear();
		}
	}

	if ( var->m_string == value )
		return var;		// Not changed

	var->m_modified = true;
	var->m_modificationCount++;

	var->m_string = value;
	var->m_value = (float) atof( var->m_string.c_str() );
	var->m_integer = (int) atoi( var->m_string.c_str() );

	return var;
}

void veCVar::setSafe( const char* varName, const char* value )
{
	int flags = veCVar::flags( varName );

	if( ( flags != VE_CVAR_NONEXISTENT ) && ( flags & VE_CVAR_PROTECTED ) ) {
		if( value )
			derr( "Restricted source tried to set \"%s\" to \"%s\"", varName, value );
		else
			derr( "Restricted source tried to modify \"%s\"", varName );
		return;
	}
	veCVar::set( varName, value );
}

void veCVar::setLatched( const char* varName, const char* value )
{
	veCVar::set2( varName, value, false );
}

void veCVar::setValue( const char *varName, float value )
{
	char val[128];
	if( value == ( int ) value ) {
		std::snprintf( val, sizeof( val ), "%i", ( int ) value );
	} else {
		std::snprintf( val, sizeof( val ), "%f", value );
	}
	veCVar::set( varName, val );
}

void veCVar::setValueSafe( const char *varName, float value )
{
	char val[128];
	if( veIsIntegral( value ) )
		std::snprintf( val, sizeof( val ), "%i", ( int ) value );
	else
		std::snprintf( val, sizeof( val ), "%f", value );
	veCVar::setSafe( varName, val );
}

float veCVar::variableValue( const char *varName )
{
	auto var = veCVar::find( varName );
	if( !var )
		return 0;
	return var->m_value;
}

int veCVar::variableIntegerValue( const char *varName )
{
	auto var = veCVar::find( varName );
	if( !var )
		return 0;
	return var->m_integer;
}

const char* veCVar::variableString( const char *varName )
{
	auto var = veCVar::find( varName );
	if( !var )
		return "";
	return var->m_string.c_str();
}

void veCVar::variableStringBuffer( const char *varName, char *buffer, int bufsize )
{
	auto var = veCVar::find( varName );
	if( !var )
		*buffer = 0;
	else
		veCopyString( buffer, bufsize, var->m_string.c_str() );
}

int veCVar::flags( const char *varName )
{
	auto var = veCVar::find( varName );
	if( !var )
		return VE_CVAR_NONEXISTENT;

	if( var->m_modified )
		return var->m_flags | VE_CVAR_MODIFIED;
	else
		return var->m_flags;
}

void veCVar::commandCompletion( std::function< void( const char *s ) > callback )
{
	for( auto& cv : m_globalCVarTable ) {
		assert( cv.second.get() );
		callback( cv.second->m_name.c_str() );
	}
}

void veCVar::reset( const char *varName )
{
	veCVar::set2( varName, nullptr, false );
}

void veCVar::forceReset( const char *varName )
{
	veCVar::set2( varName, nullptr, true );
}

void veCVar::setCheatState( void )
{
	// Set all default vars to the safe value
	for( auto& cv : m_globalCVarTable ) {

		auto var = cv.second.get();
		if( var->m_flags & VE_CVAR_CHEAT ) {

			// The CVAR_LATCHED|CVAR_CHEAT vars might escape the reset here 
			// because of a different var->latchedString
			if( var->m_latchedString.size() ) {
				var->m_latchedString.clear();
			}
			
			if( var->m_resetString != var->m_string )
				veCVar::set( var->m_name.c_str(), var->m_resetString.c_str() );
		}
	}
}

bool veCVar::command( void )
{
	auto& c = veGetCmd();

	// Check variables
	auto v = veCVar::find( c.argv( 0 ) );
	if ( !v )
		return false;
	
	// Perform a variable print or set
	if ( c.argc() == 1 ) {
		veCVar::print( v );
		return true;
	}

	// Set the value if forcing isn't required
	veCVar::set2( v->m_name.c_str(), c.args().c_str(), false );
	return true;
}

void veCVar::writeVariables( veFileData& f )
{
	std::vector< char > buffer;
	std::vector< uint8_t >& bufferData = f;

	int ptr = 0;
	buffer.resize( 8192 );
	bufferData.reserve( 65535 );

	for( auto& cv : m_globalCVarTable ) {
		auto var = cv.second.get();

		if( !var->m_name.length() )
			continue;

		if( var->m_flags & VE_CVAR_ARCHIVE ) {

			// Write the latched value, even if it hasn't taken effect yet
			if( var->m_latchedString.length() ) {
				if( var->m_name.length() + var->m_latchedString.length() + 10 > buffer.size() ) {
					dinfo( "WARNING: value of variable \"%s\" too long to write to file\n", var->m_name.c_str() );
					continue;
				}
				std::snprintf( buffer.data(), buffer.size(), "seta %s \"%s\"\n", var->m_name.c_str(), var->m_latchedString.c_str() );
			} else {
				if( var->m_name.length() + var->m_string.length() + 10 > buffer.size() ) {
					dinfo( "WARNING: value of variable \"%s\" too long to write to file\n", var->m_name.c_str() );
					continue;
				}
				std::snprintf( buffer.data(), buffer.size(), "seta %s \"%s\"\n", var->m_name.c_str(), var->m_string.c_str() );
			}

			auto slen = strlen( buffer.data() );
			for( int i = 0; i < slen; i++ ) {
				bufferData.push_back( buffer[i] );
			}
		}
	}
}


void veCVar::init( void )
{
	m_globalCVarTable.clear();
	sv_cheats = veCVar::get( "sv_cheats", "0", VE_CVAR_ROM );
}

void veCVar::checkRange( veCVar *var, float minVal, float maxVal, bool shouldBeIntegral )
{
	var->m_validate = true;
	var->m_min = minVal;
	var->m_max = maxVal;
	var->m_integral = shouldBeIntegral;

	// Force an initial range check
	veCVar::set( var->m_name.c_str(), var->m_string.c_str() );
}

void veCVar::setDescription( veCVar *var, const char *description )
{
	var->m_description = description;
}

void veCVar::restart()
{
	for( auto& cv : m_globalCVarTable ) {
		auto var = cv.second.get();
		if( !( var->m_flags & ( VE_CVAR_ROM | VE_CVAR_INIT | VE_CVAR_NORESTART ) ) ) {
			// Apply latched value if present, otherwise use reset string
			std::string latchedValue = var->m_latchedString;
			const char* newValue = latchedValue.length() > 0 ? latchedValue.c_str() : var->m_resetString.c_str();
			veCVar::set2( var->m_name.c_str(), newValue, true );
		}
	}
}


void veCVar::remove( const char* varName )
{
	auto itr = m_globalCVarTable.find( varName );
	if( itr == m_globalCVarTable.end() ) {
		return;
	}
	m_globalCVarTable.erase( itr );
}

void veCVar::updateFromIntegerFloatValues()
{
	for( auto& cv : m_globalCVarTable ) {
		auto var = cv.second.get();
		if( !( var->m_flags & VE_CVAR_ALLOW_SET_INTEGER ) ) {
			continue;
		}

		const float parsedValue = (float) atof( var->m_string.c_str() );
		const int parsedInteger = atoi( var->m_string.c_str() );
		if( var->m_value != parsedValue ) {
			if( veIsIntegral( var->m_value ) ) {
				veCVar::set( var->m_name.c_str(), veq3_va( "%d", ( int ) var->m_value ) );
			} else {
				veCVar::set( var->m_name.c_str(), veq3_va( "%f", var->m_value ) );
			}
			continue;
		}

		if( var->m_integer != parsedInteger ) {
			veCVar::set( var->m_name.c_str(), veq3_va( "%d", var->m_integer ) );
		}
	}
}

// ------------------------------------------------- Console Commands ------------------------------------------------------------

// ref: ioq3 source code

veCmd::veCmd()
{
	m_text.reserve( 128 * 1024 );
	m_line.reserve( 1024 );

	m_argv.reserve( 1024 );
	m_tokens.reserve( 1024 );
	m_cmd.reserve( 2048 );

	m_functions.reserve( 512 );
	m_aliases.reserve( 128 );
}

veCmd::~veCmd()
{
}

void veCmd::execute( veCmdExecWhen when, const char* text )
{
	switch( when ) {
		case VE_CMD_EXEC_NOW:
			if( text && strlen( text ) > 0 ) {
				this->executeString( text );
			} else {
				this->execute();
			}
			break;
		case VE_CMD_EXEC_INSERT:
			if( text && *text ) {
				if( m_text.length() ) {
					m_text.insert( 0, "\n" );
				}
				m_text.insert( 0, text );
			}
			break;
		case VE_CMD_EXEC_APPEND:
			m_text += text;
			break;
		default:
			derr_fatal( "veCmd::execute(): bad when paramter." );
	}
}

void veCmd::execute()
{
	char *text;

	// This will keep // style comments all on one line by not breaking on
	// a semicolon.  It will keep /* ... */ style comments all on one line by not
	// breaking it for semicolon or newline.
	//
	bool inStarComment = false;
	bool inSlashComment = false;

	while ( m_text.length() )
	{
		// Skip out while text still remains in buffer, leaving it
		// for next frame
		if ( m_wait > 0 ) {
			
			m_wait--;
			break;
		}

		// Find a \n or ; line break or comment: // or /* */
		text = (char *) m_text.data();
		int i;
		int quotes = 0;
		for( i = 0; i < m_text.length(); i++ ) {
			if( text[i] == '"' )
				quotes++;

			if( !( quotes & 1 ) ) {
				if( i < m_text.length() - 1 ) {
					if( !inStarComment && text[i] == '/' && text[i + 1] == '/' )
						inSlashComment = true;
					else if( !inSlashComment && text[i] == '/' && text[i + 1] == '*' )
						inStarComment = true;
					else if( inStarComment && text[i] == '*' && text[i + 1] == '/' ) {
						inStarComment = false;
						// If we are in a star comment, then the part after it is valid
						// Note: This will cause it to NUL out the terminating '/'
						// but ExecuteString doesn't require it anyway.
						i++;
						break;
					}
				}
				if( !inSlashComment && !inStarComment && text[i] == ';' )
					break;
			}

			if( !inStarComment && ( text[i] == '\n' || text[i] == '\r' ) ) {
				inSlashComment = false;
				break;
			}
		}

		m_line.resize( i + 1 );
		memcpy( m_line.data(), m_text.data(), i );
		m_line[i] = '\0';
		
		// Delete the text from the command buffer and move remaining commands down
		// this is necessary because commands (exec) can insert data at the
		// beginning of the text buffer.
		if ( i == m_text.size() ) {
			m_text.clear();
		} else {
			i++;
			m_text.erase( m_text.begin(), m_text.begin() + i );
		}

		this->tokenizeString( m_line.c_str() );
		if( this->argc() ) {
			this->executeTokenized();
		}
	}
}


// ------------------------------------------------- Console Command Exec ------------------------------------------------------------

// The functions that execute commands get their parameters with these
// functions. argv ) will return an empty string, not a NULL
// if arg > argc, so string operations are allways safe.

int veCmd::argc( void )
{
	return (int) m_argv.size();
}

const char* veCmd::argv( int arg )
{
	if( arg >= this->argc() )
		return "";
	return &m_tokens[m_argv[arg]];
}

// Returns a single string containing argv(1) to argv(argc()-1)
std::string veCmd::args( void )
{
	std::string cmdArgs;

	for ( int i = 1; i < m_argv.size(); i++ ) {
		cmdArgs += this->argv(i);
		if ( i != m_argv.size() - 1 ) {
			cmdArgs += " ";
		}
	}

	return cmdArgs;
}

std::string veCmd::argsFrom( int arg )
{
	std::string cmdArgs;

	if( arg < 0 )
		arg = 0;

	for( int i = arg; i < m_argv.size(); i++ ) {
		cmdArgs += this->argv(i);
		if( i != m_argv.size() - 1 ) {
			cmdArgs += " ";
		}
	}

	return cmdArgs;
}

const char* veCmd::cmd( void )
{
	return m_cmd.c_str();
}

// Parses the given string into command line tokens.
// The text is copied to a separate buffer and 0 characters
// are inserted in the appropriate place, The argv array
// will point into this temporary buffer.
//
void veCmd::tokenizeString2( const char *text, bool ignoreQuotes )
{
	// Clear previous args
	m_argv.clear();

	if ( !text ) {
		return;
	}

	m_cmd = text;
	auto t = text;
	m_tokens.clear();

	while ( 1 ) {
		if ( m_argv.size() >= VE_MAX_STRING_TOKENS ) {
			return;			// this is usually something malicious
		}

		while ( 1 ) {
			// Skip whitespace
			while ( *text && *text <= ' ' ) {
				text++;
			}
			if ( !*text ) {
				return;			// All tokens parsed
			}

			// skip // comments
			if ( text[0] == '/' && text[1] == '/' ) {
				return;			// All tokens parsed
			}

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) {
				while ( *text && ( text[0] != '*' || text[1] != '/' ) ) {
					text++;
				}
				if ( !*text ) {
					return;		// All tokens parsed
				}
				text += 2;
			} else {
				break;			// We are ready to parse a token
			}
		}

		// Handle quoted strings (double quotes)
		if( !ignoreQuotes && *text == '"' ) {
			m_argv.push_back( (int) m_tokens.size() );
			text++;

			while( *text && *text != '"' ) {
				m_tokens.push_back( *text++ );
			}
			m_tokens.push_back( '\0' );

			if( !*text ) {
				return;		// All tokens parsed
			}
			text++;
			continue;
		}

		// Handle single quotes
		if( !ignoreQuotes && *text == '\'' ) {
			m_argv.push_back( (int) m_tokens.size() );
			text++;

			while( *text && *text != '\'' ) {
				m_tokens.push_back( *text++ );
			}
			m_tokens.push_back( '\0' );

			if( !*text ) {
				return;		// All tokens parsed
			}
			text++;
			continue;
		}

		// Regular token
		m_argv.push_back( (int) m_tokens.size() );

		// Skip until whitespace, quote, or command
		while( *text > ' ' ) {
			if( !ignoreQuotes && text[0] == '"' ) {
				break;
			}

			if( text[0] == '/' && text[1] == '/' ) {
				break;
			}

			// Skip /* */ comments
			if( text[0] == '/' && text[1] == '*' ) {
				break;
			}

			m_tokens.push_back( *text++ );
		}
		m_tokens.push_back( '\0' );

		if ( !*text ) {
			return;		// All tokens parsed
		}
	}
}

void veCmd::tokenizeString( const char *text )
{
	this->tokenizeString2( text, false );
}

void veCmd::tokenizeStringIgnoreQuotes( const char *text )
{
	this->tokenizeString2( text, true );
}

// Parses a single line of text into arguments and tries to execute it
// as if it was typed at the console
void veCmd::executeString( const char *text )
{
	if ( !text || !*text ) {
		return;
	}

	this->execute( VE_CMD_EXEC_INSERT, text );
	this->execute();
}

void veCmd::executeTokenized()
{
	auto aliasIt = m_aliases.find( this->argv( 0 ) );
	if( aliasIt != m_aliases.end() ) {
		this->execute( VE_CMD_EXEC_INSERT, aliasIt->second.c_str() );
		return;
	}

	// Check registered command functions
	auto it = m_functions.find( this->argv( 0 ) );
	if( it != m_functions.end() ) {
		if ( it->second.func )
			it->second.func();
		return;
	}

	// Check cvars
	if ( veCVar::command() )
		return;

	// TODO: client, server, ui commands!
	auto ass = this->argv( 0 );
	dinfo( "%s: unknown command.\n", ass );
}

// Called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_clientCommand instead of executed locally.
//
void veCmd::addCommand( const char *name, veCmdFunc function )
{
	if( m_functions.find( name ) != m_functions.end() ) {
		dinfo( "veCmd::addCommand(): %s already defined!\n", name );
		return;
	}

	veCmdFuncHandler handler;
	handler.name = name;
	handler.func = function;

	m_functions[name] = handler;
}

void veCmd::removeCommand( const char *name )
{
	auto it = m_functions.find( name );
	if( it != m_functions.end() ) {
		m_functions.erase( it );
	}
}

void veCmd::commandCompletion( std::function< void( const char *s, const char *expr ) > callback )
{
	for( auto& it : m_functions ) {
		callback( it.first.c_str(), it.second.complete.length() ? it.second.complete.c_str() : nullptr );
	}

	for( auto& alias : m_aliases ) {
		callback( alias.first.c_str(), nullptr );
	}
}

void veCmd::setCommandCompletion( const char *command, const std::string& complete )
{
	m_functions[command].complete = complete;
}

const char* veCmd::stringContains( const char *str1, const char *str2, int caseSensitive )
{
	int len, i, j;

	len = (int) strlen( str1 ) - (int) strlen( str2 );
	for( i = 0; i <= len; i++, str1++ ) {
		for( j = 0; str2[j]; j++ ) {
			if( caseSensitive ) {
				if( str1[j] != str2[j] ) {
					break;
				}
			} else {
				if( toupper( str1[j] ) != toupper( str2[j] ) ) {
					break;
				}
			}
		}
		if( !str2[j] ) {
			return str1;
		}
	}
	return NULL;
}

int veCmd::filter( const char *filter, const char *name, int caseSensitive )
{
	static char buf[ VE_MAX_TOKEN_LENGTH ];
	const char *ptr;
	int i, found;

	while( *filter ) {
		if( *filter == '*' ) {
			filter++;
			for( i = 0; *filter; i++ ) {
				if( *filter == '*' || *filter == '?' ) break;
				if ( i >= sizeof( buf ) ) derr( "veCmd::filter buffer overflow! Bump VE_MAX_TOKEN_LENGTH?\n" );
				buf[i] = *filter;
				filter++;
			}
			if ( i >= sizeof( buf ) ) derr( "veCmd::filter buffer overflow! Bump VE_MAX_TOKEN_LENGTH?\n" );
			buf[i] = '\0';
			if( strlen( buf ) ) {
				ptr = veCmd::stringContains( name, buf, caseSensitive );
				if( !ptr ) return false;
				name = ptr + strlen( buf );
			} else {
				// Trailing * matches any remaining characters
				while( *name ) name++;
			}
		} else if( *filter == '?' ) {
			filter++;
			if( *name == '\0' ) return false;  // No character to match
			name++;
		} else if( *filter == '[' && *( filter + 1 ) == '[' ) {
			filter++;
		} else if( *filter == '[' ) {
			filter++;
			found = false;
			while( *filter && !found ) {
				if( *filter == ']' && *( filter + 1 ) != ']' ) break;
				if( *( filter + 1 ) == '-' && *( filter + 2 ) && ( *( filter + 2 ) != ']' || *( filter + 3 ) == ']' ) ) {
					if( caseSensitive ) {
						if( *name >= *filter && *name <= *( filter + 2 ) ) found = true;
					} else {
						if( toupper( *name ) >= toupper( *filter ) &&
							toupper( *name ) <= toupper( *( filter + 2 ) ) ) found = true;
					}
					filter += 3;
				} else {
					if( caseSensitive ) {
						if( *filter == *name ) found = true;
					} else {
						if( toupper( *filter ) == toupper( *name ) ) found = true;
					}
					filter++;
				}
			}
			if( !found ) return false;
			while( *filter ) {
				if( *filter == ']' && *( filter + 1 ) != ']' ) break;
				filter++;
			}
			filter++;
			name++;
		} else {
			if( caseSensitive ) {
				if( *filter != *name ) return false;
			} else {
				if( toupper( *filter ) != toupper( *name ) ) return false;
			}
			filter++;
			name++;
		}
	}
	// Filter is exhausted - name must also be exhausted for a match
	return *name == '\0';
}

// ---------------------------------- Default Console Command Functions --------------------------

// Causes execution of the remainder of the command buffer to be delayed until
// next frame.  This allows commands like:
// bind g "cmd use rocket ; +attack ; wait ; -attack ; cmd use blaster"
//
void veCmd_WaitFunc( void )
{
	auto& c = veGetCmd();
	if( c.argc() == 2 ) {
		c.setWait( atoi( c.argv( 1 ) ) );
		if( c.getWait() < 0 )
			c.setWait( 1 ); // Ignore the argument.
	} else {
		c.setWait( 1 );
	}
}

void veCmd_EchoFunc()
{
	dinfo( "%s\n", veGetCmd().args().c_str() );
}

void veCmd_AliasFunc()
{
	auto& c = veGetCmd();

	if( c.argc() == 1 ) {
		for( const auto& alias : c.getAliases() ) {
			dinfo( "%s : %s\n", alias.first.c_str(), alias.second.c_str() );
		}
		dinfo( "%d aliases\n", c.getAliases().size() );
		return;
	}

	auto name = c.argv( 1 );
	if( !veIsValidConsoleToken( name ) || strcmp( name, "alias" ) == 0 ) {
		dinfo( "alias: invalid alias name '%s'\n", name );
		return;
	}

	if( c.argc() == 2 ) {
		auto it = c.getAliases().find( name );
		if( it == c.getAliases().end() ) {
			dinfo( "alias %s does not exist.\n", name );
			return;
		}

		dinfo( "%s : %s\n", it->first.c_str(), it->second.c_str() );
		return;
	}

	c.getAliases()[name] = c.argsFrom( 2 );
}

void veCmd_ExecFunc()
{
	auto& c = veGetCmd();
	if( c.argc() != 2 ) {
		dinfo( "usage: exec <filename>\n" );
		return;
	}

	std::ifstream file( c.argv( 1 ), std::ios::binary );
	if( !file ) {
		dinfo( "couldn't exec %s\n", c.argv( 1 ) );
		return;
	}

	std::string contents( ( std::istreambuf_iterator< char >( file ) ), std::istreambuf_iterator< char >() );
	if( contents.empty() ) {
		return;
	}

	c.execute( VE_CMD_EXEC_INSERT, contents.c_str() );
}

void veCmd_ListFunc()
{
	auto& c = veGetCmd();
	auto match = ( c.argc() > 1 ) ? c.argv( 1 ) : nullptr;
	auto funcList = c.getFunctionsList();
	
	for ( auto& it : funcList ) {
		if ( match && !veCmd::filter( match, it.second.name.c_str(), false ) )
			continue;
		dinfo( "%s\n", it.second.name.c_str() );
	}

	dinfo( "%d commands\n", funcList.size() );
}

void veCVarCmd_PrintFunc()
{
	auto& c = veGetCmd();
	if ( c.argc() != 2 ) {
		dinfo( "usage: print <variable>\n" );
		return;
	}
	auto name = c.argv( 1 );
	auto cv = veCVar::find( name );
	if ( cv )
		veCVar::print( cv );
	else
		dinfo( "Cvar %s does not exist.\n", name );
}

void veCVarCmd_ToggleFunc()
{
	auto& c = veGetCmd();

	if( c.argc() < 2 ) {
		dinfo( "usage: toggle <variable> [value1, value2, ...]\n" );
		return;
	}

	if( c.argc() == 2 ) {
		veCVar::set2( c.argv( 1 ), veq3_va( "%d", !veCVar::variableValue( c.argv( 1 ) ) ), false );
		return;
	}

	if( c.argc() == 3 ) {
		dinfo( "toggle: nothing to toggle to\n" );
		return;
	}

	auto curval = veCVar::variableString( c.argv( 1 ) );

	// Don't bother checking the last arg for a match since the desired behaviour is the same as no match (set to the first argument)
	for( int i = 2; i + 1 < c.argc(); i++ ) {
		if( strcmp( curval, c.argv( i ) ) == 0 ) {
			veCVar::set2( c.argv( 1 ), c.argv( i + 1 ), false );
			return;
		}
	}

	// Fallback
	veCVar::set2( c.argv( 1 ), c.argv( 2 ), false );
}

void veCVarCmd_SetFunc()
{
	auto& c = veGetCmd();
	auto ac = c.argc();
	auto cmd = c.argv( 0 );

	if( ac < 2 ) {
		dinfo( "usage: %s <variable> <value>\n", cmd );
		return;
	}
	if( ac == 2 ) {
		veCVarCmd_PrintFunc();
		return;
	}

	auto v = veCVar::set2( c.argv( 1 ), c.argsFrom( 2 ).c_str(), false );
	if( !v ) {
		return;
	}

	switch( cmd[3] ) {
		case 'a':
			if( !( v->getFlags() & VE_CVAR_ARCHIVE ) ) {
				v->getFlags() |= VE_CVAR_ARCHIVE;
				veCVar::getModifiedFlags() |= VE_CVAR_ARCHIVE;
			}
			break;
		case 'u':
			if( !( v->getFlags() & VE_CVAR_USERINFO ) ) {
				v->getFlags() |= VE_CVAR_USERINFO;
				veCVar::getModifiedFlags() |= VE_CVAR_USERINFO;
			}
			break;
		case 's':
			if( !( v->getFlags() & VE_CVAR_SERVERINFO ) ) {
				v->getFlags() |= VE_CVAR_SERVERINFO;
				veCVar::getModifiedFlags() |= VE_CVAR_SERVERINFO;
			}
			break;
		case 'e': // set command
			// No special flags to set
			break;
		case 't': // reset command
			// No special flags to set
			break;
		default:
			// Unknown command, do nothing
			break;
	}
}

void veCVarCmd_ResetFunc()
{
	auto& c = veGetCmd();
	if( c.argc() != 2 ) {
		dinfo( "usage: reset <variable>\n" );
		return;
	}
	veCVar::reset( c.argv( 1 ) );
}

void veCVarCmd_ListFunc()
{
	auto& c = veGetCmd();

	const char* match = nullptr;
	if ( c.argc() > 1 ) {
		match = c.argv( 1 );
	}

	veCVar::list( match );
}

void veCVarCmd_ListModifiedFunc()
{
	auto& c = veGetCmd();

	const char* match = nullptr;
	if ( c.argc() > 1 ) {
		match = c.argv( 1 );
	}

	veCVar::listModified( match );
}

void veCVarCmd_RestartFunc()
{
	veCVar::restart();
}

void veCVar_InitCmd()
{
	auto& c = veGetCmd();
	
	c.addCommand( "alias", veCmd_AliasFunc );
	c.addCommand( "print", veCVarCmd_PrintFunc );
	c.setCommandCompletion( "print", "C_V_" );
	c.addCommand( "toggle", veCVarCmd_ToggleFunc );
	c.setCommandCompletion( "toggle", "C_V_" );
	
	c.addCommand( "set", veCVarCmd_SetFunc );
	c.addCommand( "sets", veCVarCmd_SetFunc );
	c.addCommand( "setu", veCVarCmd_SetFunc );
	c.addCommand( "seta", veCVarCmd_SetFunc );
	c.addCommand( "reset", veCVarCmd_ResetFunc );
	c.setCommandCompletion( "set", "C_V_" );
	c.setCommandCompletion( "sets", "C_V_" );
	c.setCommandCompletion( "setu", "C_V_" );
	c.setCommandCompletion( "seta", "C_V_" );
	c.setCommandCompletion( "reset", "C_V_" );

	c.addCommand( "cvarlist", veCVarCmd_ListFunc );
	c.addCommand( "cvar_list", veCVarCmd_ListFunc );
	c.addCommand( "cvar_modified", veCVarCmd_ListModifiedFunc );
	c.addCommand( "cvar_restart", veCVarCmd_RestartFunc );
}

void veCmd_InitDefaultFunctions()
{
	static bool veCmdDefaultFunctionsInitialised = false;
	if ( veCmdDefaultFunctionsInitialised )
		return;
	veCmdDefaultFunctionsInitialised = true;

	auto& c = veGetCmd();
	c.addCommand( "cmdlist", veCmd_ListFunc );
	c.addCommand( "echo", veCmd_EchoFunc );
	c.addCommand( "exec", veCmd_ExecFunc );
	c.addCommand( "wait", veCmd_WaitFunc );

	veCVar_InitCmd();
}

// ------------------------------------------------- Misc. Other Utils -------------------------------------------------

// va
// Does a varargs printf into a temp buffer, so I don't need to have
// varargs versions of all text functions.
//
char* veq3_va( const char *format, ... )
{
	va_list		argptr;
	static thread_local std::array< std::array< char, 8192 >, 4 > string;
	static thread_local size_t index = 0;
	char		*buf;

	buf = string[index % string.size()].data();
	index++;

	va_start( argptr, format );
	vsnprintf( buf, string.front().size(), format, argptr );
	va_end( argptr );

	return buf;
}
