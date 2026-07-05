/*
	vvar — a console-variable / command / info-string library.

	Copyright (c) 2026 Xi Ma Chen
	Released under the MIT License. See the LICENSE file for the full text.
*/



#include "vvar.h"

#include <array>
#include <fstream>
#include <sstream>

veCVar* sv_cheats = nullptr;
std::unique_ptr< veCmd > g_cmd;

static std::string g_fieldCompletionString;
static std::string g_fieldShortestMatch;
static int g_fieldMatchCount;

// True iff the string contains none of the disallowed characters
// (backslash, double quote, semicolon). Used to validate info-string keys
// and values during serialization, and to validate cvar names.
static bool veInfoStringValueIsValid( const std::string& value )
{
	for ( char c : value )
	{
		if ( c == '\\' || c == '"' || c == ';' )
			return false;
	}
	return true;
}

// True iff the given C-string is a non-empty console token whose characters
// are all allowed (no '\\', '"', ';').
static bool veIsValidConsoleToken( const char* value )
{
	if ( !value || *value == '\0' )
		return false;
	for ( const char* p = value; *p; ++p )
	{
		if ( *p == '\\' || *p == '"' || *p == ';' )
			return false;
	}
	return true;
}

// Bounded string copy into a caller-supplied buffer. No-op for null buffers
// or non-positive sizes; null value copies an empty string.
static void veCopyString( char* buffer, int bufsize, const char* value )
{
	if ( !buffer || bufsize <= 0 )
		return;
	if ( !value )
	{
		buffer[ 0 ] = '\0';
		return;
	}
	std::snprintf( buffer, static_cast< size_t >( bufsize ), "%s", value );
}

// ------------------------------------ Key / value info strings ----------------------------------------


std::unordered_map< std::string, std::unordered_map< std::string, std::string > > veIVar::m_globalIVarTable;
thread_local std::string veIVar::m_serializedInfoString;

// Look up the value for `key` within section `s`. Returns nullptr if the
// section or key is missing. Null arguments are treated as empty strings.
const char* veIVar::get( const char *s, const char *key )
{
	if ( !s ) s = "";
	if ( !key ) key = "";
	auto secIt = m_globalIVarTable.find( s );
	if ( secIt == m_globalIVarTable.end() )
		return nullptr;
	auto keyIt = secIt->second.find( key );
	if ( keyIt == secIt->second.end() )
		return nullptr;
	return keyIt->second.c_str();
}

// Remove `key` from section `s`. If the section becomes empty it is erased.
void veIVar::remove( const char *s, const char *key )
{
	if ( !s ) s = "";
	if ( !key ) key = "";
	auto secIt = m_globalIVarTable.find( s );
	if ( secIt == m_globalIVarTable.end() )
		return;
	secIt->second.erase( key );
	if ( secIt->second.empty() )
		m_globalIVarTable.erase( secIt );
}

// Store `value` under `key` in section `s`, creating section/key as needed.
// No validation is performed here; invalid characters are filtered later
// during serialization.
void veIVar::set( const char *s, const char *key, const char *value )
{
	if ( !s ) s = "";
	if ( !key ) key = "";
	if ( !value ) value = "";
	m_globalIVarTable[ s ][ key ] = value;
}

// Serialize section `s` as repeated "\\key\\value". Pairs whose key or value
// contains any disallowed character are skipped. The result is stored in a
// thread-local buffer that is valid until the next toString() on the same
// thread.
const char* veIVar::toString( const char *s )
{
	if ( !s ) s = "";
	m_serializedInfoString.clear();
	auto secIt = m_globalIVarTable.find( s );
	if ( secIt == m_globalIVarTable.end() )
		return m_serializedInfoString.c_str();

	for ( const auto& kv : secIt->second )
	{
		if ( !veInfoStringValueIsValid( kv.first ) )
			continue;
		if ( !veInfoStringValueIsValid( kv.second ) )
			continue;
		m_serializedInfoString += '\\';
		m_serializedInfoString += kv.first;
		m_serializedInfoString += '\\';
		m_serializedInfoString += kv.second;
	}
	return m_serializedInfoString.c_str();
}

// Parse a backslash-delimited key/value stream and replace the contents of
// section `s`. A leading '\' is tolerated but not required. Pairs with empty
// keys, invalid characters, or trailing keys without values are dropped.
// Duplicate keys: last value wins. Null or empty infoString erases the
// section entirely.
void veIVar::fromString( const char *s, const char *infoString )
{
	if ( !s ) s = "";

	// Always replace the section's contents.
	m_globalIVarTable.erase( s );
	if ( !infoString || *infoString == '\0' )
		return;

	// A single leading backslash is conventional but optional.
	if ( *infoString == '\\' )
		++infoString;

	enum Mode { MODE_KEY, MODE_VALUE };
	Mode mode = MODE_KEY;
	std::string key;
	std::string value;
	bool keyNonEmpty = false;
	bool valueNonEmpty = false;
	auto flushPairIfValid = [&]()
	{
		if ( keyNonEmpty && valueNonEmpty
			&& veInfoStringValueIsValid( key )
			&& veInfoStringValueIsValid( value ) )
		{
			m_globalIVarTable[ s ][ key ] = value;
		}
		key.clear();
		value.clear();
		keyNonEmpty = false;
		valueNonEmpty = false;
	};

	for ( const char* p = infoString; *p; ++p )
	{
		char c = *p;
		if ( c == '\\' )
		{
			if ( mode == MODE_KEY )
			{
				// End of key (whether empty or not). Switch to value mode.
				mode = MODE_VALUE;
				valueNonEmpty = false;
			}
			else
			{
				// End of value: finalize pair and prepare for next key.
				flushPairIfValid();
				mode = MODE_KEY;
			}
		}
		else if ( mode == MODE_KEY )
		{
			key.push_back( c );
			keyNonEmpty = true;
		}
		else
		{
			value.push_back( c );
			valueNonEmpty = true;
		}
	}

	// If we ended mid-value with both key and value present, store the pair.
	// Otherwise any trailing key without a value is dropped.
	if ( mode == MODE_VALUE && keyNonEmpty && valueNonEmpty )
		flushPairIfValid();
}

// ------------------------------------ Console Variables Implementation ----------------------------------------


std::map< std::string, std::unique_ptr< veCVar > > veCVar::m_globalCVarTable;
int veCVar::m_modifiedFlags = 0;

// Emit one character per flag slot for a cvar. Uses the flag's letter when
// the bit is set, otherwise a space. Order:
//   S s U R I A L C
// (SERVERINFO, SYSTEMINFO, USERINFO, ROM, INIT, ARCHIVE, LATCH, CHEAT)
static void veCVarCmd_PrintVarFlags( veCVar* var )
{
	int flags = var->getFlags();
	dinfo( "%c%c%c%c%c%c%c%c",
		( flags & VE_CVAR_SERVERINFO ) ? 'S' : ' ',
		( flags & VE_CVAR_SYSTEMINFO ) ? 's' : ' ',
		( flags & VE_CVAR_USERINFO   ) ? 'U' : ' ',
		( flags & VE_CVAR_ROM        ) ? 'R' : ' ',
		( flags & VE_CVAR_INIT       ) ? 'I' : ' ',
		( flags & VE_CVAR_ARCHIVE    ) ? 'A' : ' ',
		( flags & VE_CVAR_LATCH      ) ? 'L' : ' ',
		( flags & VE_CVAR_CHEAT      ) ? 'C' : ' ' );
}

// Private default constructor: cvar instances are only allocated inside veCVar::get().
veCVar::veCVar()
{
}

veCVar::~veCVar()
{
}

// True iff `s` is non-null and contains none of '\\', '"', ';'.
bool veCVar::validateString( const char *s )
{
	if ( !s )
		return false;
	for ( const char* p = s; *p; ++p )
	{
		if ( *p == '\\' || *p == '"' || *p == ';' )
			return false;
	}
	return true;
}

// Validate a value against the cvar's range/format constraints. Returns
// either the original `value` or a pointer to a static formatted buffer when
// the value was clamped/coerced. `warn` controls whether diagnostic messages
// are emitted.
const char* veCVar::validate( veCVar *var, const char *value, bool warn )
{
	if ( !var || !var->m_validate )
		return value;
	if ( !value )
		return value;

	float f = 0.0f;
	bool changed = false;
	const char* name = var->m_name.c_str();

	if ( veIsANumber( value ) )
	{
		f = static_cast< float >( std::atof( value ) );
		if ( var->m_integral && !veIsIntegral( f ) )
		{
			if ( warn )
				derr( "WARNING: cvar '%s' must be integral", name );
			f = static_cast< float >( static_cast< int >( f ) );
			changed = true;
		}
	}
	else
	{
		if ( warn )
			derr( "WARNING: cvar '%s' must be numeric", name );
		f = static_cast< float >( std::atof( var->m_resetString.c_str() ) );
		changed = true;
	}

	if ( f < var->m_min )
	{
		if ( warn )
		{
			if ( veIsIntegral( var->m_min ) )
				derr( "WARNING: cvar '%s' out of range (min %d)", name, static_cast< int >( var->m_min ) );
			else
				derr( "WARNING: cvar '%s' out of range (min %f)", name, var->m_min );
		}
		f = var->m_min;
		changed = true;
	}
	if ( f > var->m_max )
	{
		if ( warn )
		{
			if ( veIsIntegral( var->m_max ) )
				derr( "WARNING: cvar '%s' out of range (max %d)", name, static_cast< int >( var->m_max ) );
			else
				derr( "WARNING: cvar '%s' out of range (max %f)", name, var->m_max );
		}
		f = var->m_max;
		changed = true;
	}

	if ( !changed )
		return value;

	static thread_local char formatted[ VE_MAX_CVAR_VALUE_STRING ];
	if ( veIsIntegral( f ) )
		std::snprintf( formatted, sizeof( formatted ), "%d", static_cast< int >( f ) );
	else
		std::snprintf( formatted, sizeof( formatted ), "%f", static_cast< double >( f ) );
	return formatted;
}

veCVar* veCVar::find( const char *varName )
{
	if ( !varName )
		return nullptr;
	auto it = m_globalCVarTable.find( varName );
	if ( it == m_globalCVarTable.end() )
		return nullptr;
	return it->second.get();
}

// Register or look up a cvar while preserving the public compatibility contract.
veCVar* veCVar::get( const char* varName, const char* value, int flags )
{
	if ( !varName || !value )
	{
		derr( "veCVar::get() NULL parameter!\n" );
		return nullptr;
	}

	bool nameInvalid = false;
	if ( !validateString( varName ) )
	{
		derr( "invalid cvar name string: %s\n", varName );
		nameInvalid = true;
		varName = "BADNAME";
	}

	// Pre-validate the incoming value so existing cvars keep their formatted
	// representation when the same value is re-registered.
	value = validate( find( varName ), value, false );

	auto it = m_globalCVarTable.find( varName );
	if ( it != m_globalCVarTable.end() )
	{
		veCVar* existing = it->second.get();

		// Reset-string reconciliation.
		if ( existing->m_resetString.empty() )
		{
			existing->m_resetString = value;
		}
		else if ( value[ 0 ] != '\0' && existing->m_resetString != value )
		{
			dinfo( "veCVar::get() Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n",
				existing->m_name.c_str(),
				existing->m_resetString.c_str(),
				value );
		}

		// Apply any pending latched value.
		if ( !existing->m_latchedString.empty() )
		{
			std::string captured = existing->m_latchedString;
			existing->m_latchedString.clear();
			set2( existing->m_name.c_str(), captured.c_str(), true );
		}

		existing->m_flags |= flags;
		m_modifiedFlags |= flags;
		return existing;
	}

	// Allocate a new cvar.
	auto cv = std::unique_ptr< veCVar >( new ( std::nothrow ) veCVar() );
	if ( !cv )
		derr_fatal( "veCVar::get() allocation failure for \"%s\"\n", varName );

	cv->m_name = varName;
	cv->m_string = nameInvalid ? "BADNAME" : value;
	cv->m_modified = false;
	cv->m_modificationCount = 0;
	cv->m_value = static_cast< float >( std::atof( cv->m_string.c_str() ) );
	cv->m_integer = std::atoi( cv->m_string.c_str() );
	cv->m_resetString = value;
	cv->m_validate = false;
	cv->m_flags = flags;
	m_modifiedFlags |= flags;

	veCVar* raw = cv.get();
	m_globalCVarTable.emplace( varName, std::move( cv ) );
	return raw;
}

// Print a human-readable summary of a single cvar. Always emits the current
// name/value; adds "the default" or "default: ..." when not read-only;
// appends latched string and description when present.
void veCVar::print( veCVar *v )
{
	const char* name = v->m_name.c_str();
	const char* string = v->m_string.c_str();
	dinfo( "\"%s\" is:\"%s\"", name, string );
	if ( !( v->m_flags & VE_CVAR_ROM ) )
	{
		if ( v->m_string == v->m_resetString )
		{
			dinfo( ", the default" );
		}
		else
		{
			dinfo( " default:\"%s\"", v->m_resetString.c_str() );
		}
	}
	dinfo( "\n" );
	if ( !v->m_latchedString.empty() )
		dinfo( "latched: \"%s\"\n", v->m_latchedString.c_str() );
	if ( !v->m_description.empty() )
		dinfo( "%s\n", v->m_description.c_str() );
}

// List all cvars (optionally filtered by glob), one per line with flag
// letters and "name \"value\"" formatting, then a total count.
//
// Filter controls which rows are listed; the "total cvars" count always
// reflects every registered cvar regardless of the filter.
void veCVar::list( const char* match )
{
	for ( const auto& kv : m_globalCVarTable )
	{
		const std::string& name = kv.first;
		veCVar* var = kv.second.get();
		if ( match && !veCmd::filter( match, name.c_str(), false ) )
			continue;
		veCVarCmd_PrintVarFlags( var );
		dinfo( " %s \"%s\"\n", name.c_str(), var->m_string.c_str() );
	}
	dinfo( "\n%i total cvars\n", static_cast< int >( m_globalCVarTable.size() ) );
}

// List cvars whose effective value differs from the reset string (with
// optional glob filter), followed by a total count of all modified cvars.
//
// Filter controls which rows are listed; the "total modified cvars" count
// always reflects every modified cvar regardless of the filter.
void veCVar::listModified( const char* match )
{
	int modifiedTotal = 0;
	for ( const auto& kv : m_globalCVarTable )
	{
		veCVar* var = kv.second.get();
		const std::string& effective = var->m_latchedString.empty() ? var->m_string : var->m_latchedString;
		if ( var->m_modificationCount == 0 || effective == var->m_resetString )
			continue;
		++modifiedTotal;
		if ( match && !veCmd::filter( match, kv.first.c_str(), false ) )
			continue;
		veCVarCmd_PrintVarFlags( var );
		dinfo( " %s \"%s\", default \"%s\"\n",
			kv.first.c_str(), effective.c_str(), var->m_resetString.c_str() );
	}
	dinfo( "\n%i total modified cvars\n", modifiedTotal );
}

void veCVar::set( const char* varName, const char* value )
{
	set2( varName, value, true );
}

// Core mutation path for cvar value changes, protections, latching, and coercion.
veCVar* veCVar::set2( const char* varName, const char* value, bool force )
{
	if ( !validateString( varName ) )
	{
		dinfo( "invalid cvar name string: %s\n", varName );
		varName = "BADNAME";
	}

	veCVar* var = find( varName );
	if ( !var )
	{
		if ( !value )
			return nullptr;
		return get( varName, value, 0 );
	}

	if ( !value )
		value = var->m_resetString.c_str();

	value = validate( var, value, true );

	// Early-equality short-circuit happens BEFORE force/latch handling.
	if ( var->m_string == value )
		return var;

	m_modifiedFlags |= var->m_flags;

	if ( !force )
	{
		if ( var->m_flags & VE_CVAR_ROM )
		{
			dinfo( "%s is read only.\n", var->m_name.c_str() );
			return var;
		}
		if ( var->m_flags & VE_CVAR_INIT )
		{
			dinfo( "%s is write protected.\n", var->m_name.c_str() );
			return var;
		}
		if ( ( var->m_flags & VE_CVAR_CHEAT ) && sv_cheats && sv_cheats->getInteger() == 0 )
		{
			dinfo( "%s is cheat protected.\n", var->m_name.c_str() );
			return var;
		}
		if ( var->m_flags & VE_CVAR_LATCH )
		{
			if ( !var->m_latchedString.empty() )
			{
				if ( var->m_latchedString == value )
					return var;
				var->m_latchedString.clear();
			}
			else if ( var->m_string == value )
			{
				return var;
			}
			dinfo( "%s will be changed upon restarting.\n", var->m_name.c_str() );
			var->m_latchedString = value;
			var->m_modified = true;
			var->m_modificationCount++;
			return var;
		}
	}
	else
	{
		var->m_latchedString.clear();
	}

	if ( var->m_string == value )
		return var;

	var->m_modified = true;
	var->m_modificationCount++;
	var->m_string = value;
	var->m_value = static_cast< float >( std::atof( var->m_string.c_str() ) );
	var->m_integer = std::atoi( var->m_string.c_str() );
	return var;
}

void veCVar::setSafe( const char* varName, const char* value )
{
	int existingFlags = flags( varName );
	if ( !( existingFlags & VE_CVAR_NONEXISTENT ) && ( existingFlags & VE_CVAR_PROTECTED ) )
	{
		if ( value )
			derr( "Restricted source tried to set \"%s\" to \"%s\"", varName, value );
		else
			derr( "Restricted source tried to modify \"%s\"", varName );
		return;
	}
	set( varName, value );
}

void veCVar::setLatched( const char* varName, const char* value )
{
	set2( varName, value, false );
}

void veCVar::setValue( const char *varName, float value )
{
	char buf[ VE_MAX_CVAR_VALUE_STRING ];
	if ( veIsIntegral( value ) )
		std::snprintf( buf, sizeof( buf ), "%i", static_cast< int >( value ) );
	else
		std::snprintf( buf, sizeof( buf ), "%f", static_cast< double >( value ) );
	set( varName, buf );
}

void veCVar::setValueSafe( const char *varName, float value )
{
	char buf[ VE_MAX_CVAR_VALUE_STRING ];
	if ( veIsIntegral( value ) )
		std::snprintf( buf, sizeof( buf ), "%i", static_cast< int >( value ) );
	else
		std::snprintf( buf, sizeof( buf ), "%f", static_cast< double >( value ) );
	setSafe( varName, buf );
}

float veCVar::variableValue( const char *varName )
{
	veCVar* var = find( varName );
	if ( !var )
		return 0.0f;
	return var->m_value;
}

int veCVar::variableIntegerValue( const char *varName )
{
	veCVar* var = find( varName );
	if ( !var )
		return 0;
	return var->m_integer;
}

const char* veCVar::variableString( const char *varName )
{
	veCVar* var = find( varName );
	if ( !var )
		return "";
	return var->m_string.c_str();
}

void veCVar::variableStringBuffer( const char *varName, char *buffer, int bufsize )
{
	if ( !buffer || bufsize <= 0 )
		return;
	veCVar* var = find( varName );
	if ( !var )
	{
		buffer[ 0 ] = '\0';
		return;
	}
	veCopyString( buffer, bufsize, var->m_string.c_str() );
}

int veCVar::flags( const char *varName )
{
	veCVar* var = find( varName );
	if ( !var )
		return VE_CVAR_NONEXISTENT;
	int f = var->m_flags;
	if ( var->m_modified )
		f |= VE_CVAR_MODIFIED;
	return f;
}

void veCVar::commandCompletion( std::function< void( const char *s ) > callback )
{
	for ( const auto& kv : m_globalCVarTable )
		callback( kv.first.c_str() );
}

void veCVar::reset( const char *varName )
{
	set2( varName, nullptr, false );
}

void veCVar::forceReset( const char *varName )
{
	set2( varName, nullptr, true );
}

void veCVar::setCheatState( void )
{
	for ( auto& kv : m_globalCVarTable )
	{
		veCVar* var = kv.second.get();
		if ( !( var->m_flags & VE_CVAR_CHEAT ) )
			continue;
		var->m_latchedString.clear();
		if ( var->m_resetString != var->m_string )
			set( var->m_name.c_str(), var->m_resetString.c_str() );
	}
}

// Console fallback for unknown commands: handle cvar-style references.
bool veCVar::command( void )
{
	auto& cmd = veGetCmd();
	veCVar* v = find( cmd.argv( 0 ) );
	if ( !v )
		return false;
	if ( cmd.argc() == 1 )
	{
		print( v );
		return true;
	}
	set2( v->m_name.c_str(), cmd.args().c_str(), false );
	return true;
}

// Write `seta NAME "VALUE"` lines for every ARCHIVE cvar into `f`. Uses
// the latched string when present. Skips cvars whose name+value wouldn't
// fit in an 8192-byte temp buffer.
void veCVar::writeVariables( veFileData& f )
{
	char buffer[ 8192 ];
	for ( const auto& kv : m_globalCVarTable )
	{
		veCVar* var = kv.second.get();
		if ( !( var->m_flags & VE_CVAR_ARCHIVE ) )
			continue;
		const std::string& value = var->m_latchedString.empty() ? var->m_string : var->m_latchedString;
		int needed = static_cast< int >( kv.first.size() ) + static_cast< int >( value.size() ) + 10;
		if ( needed >= static_cast< int >( sizeof( buffer ) ) )
		{
			dinfo( "WARNING: value of variable \"%s\" too long to write to file\n", kv.first.c_str() );
			continue;
		}
		int n = std::snprintf( buffer, sizeof( buffer ), "seta %s \"%s\"\n", kv.first.c_str(), value.c_str() );
		if ( n > 0 )
			f.insert( f.end(), reinterpret_cast< uint8_t* >( buffer ), reinterpret_cast< uint8_t* >( buffer ) + n );
	}
}

void veCVar::init( void )
{
	m_globalCVarTable.clear();
	m_modifiedFlags = 0;
	sv_cheats = get( "sv_cheats", "0", VE_CVAR_ROM );
}

void veCVar::checkRange( veCVar *var, float minVal, float maxVal, bool shouldBeIntegral )
{
	var->m_validate = true;
	var->m_min = minVal;
	var->m_max = maxVal;
	var->m_integral = shouldBeIntegral;
	set( var->m_name.c_str(), var->m_string.c_str() );
}

void veCVar::setDescription( veCVar *var, const char *description )
{
	if ( !var )
		return;
	var->m_description = description ? description : "";
}

void veCVar::restart()
{
	for ( auto& kv : m_globalCVarTable )
	{
		veCVar* var = kv.second.get();
		if ( var->m_flags & ( VE_CVAR_ROM | VE_CVAR_INIT | VE_CVAR_NORESTART ) )
			continue;
		if ( !var->m_latchedString.empty() )
		{
			// Pass a copy because set2 may clear the latched string when force=true.
			std::string captured = var->m_latchedString;
			set2( var->m_name.c_str(), captured.c_str(), true );
		}
		else
		{
			set2( var->m_name.c_str(), var->m_resetString.c_str(), true );
		}
	}
}

void veCVar::remove( const char* varName )
{
	if ( !varName )
		return;
	m_globalCVarTable.erase( varName );
}

void veCVar::updateFromIntegerFloatValues()
{
	for ( auto& kv : m_globalCVarTable )
	{
		veCVar* var = kv.second.get();
		if ( !( var->m_flags & VE_CVAR_ALLOW_SET_INTEGER ) )
			continue;
		if ( var->m_value != static_cast< float >( std::atof( var->m_string.c_str() ) ) )
		{
			char buf[ VE_MAX_CVAR_VALUE_STRING ];
			if ( veIsIntegral( var->m_value ) )
				std::snprintf( buf, sizeof( buf ), "%d", static_cast< int >( var->m_value ) );
			else
				std::snprintf( buf, sizeof( buf ), "%f", static_cast< double >( var->m_value ) );
			set( var->m_name.c_str(), buf );
		}
		else if ( var->m_integer != std::atoi( var->m_string.c_str() ) )
		{
			char buf[ VE_MAX_CVAR_VALUE_STRING ];
			std::snprintf( buf, sizeof( buf ), "%d", var->m_integer );
			set( var->m_name.c_str(), buf );
		}
	}
}

// ------------------------------------------------- Console Commands ------------------------------------------------------------


// veCmd constructor: reserve capacity to make the command buffer more
// efficient when many small commands are parsed.
veCmd::veCmd()
{
	m_argv.reserve( 64 );
	m_tokens.reserve( 1024 );
}

veCmd::~veCmd()
{
}

// Dispatch a piece of text to the command buffer / immediate execution.
void veCmd::execute( veCmdExecWhen when, const char* text )
{
	switch ( when )
	{
	case VE_CMD_EXEC_NOW:
		if ( text && text[ 0 ] != '\0' )
			executeString( text );
		else
			execute();
		break;
	case VE_CMD_EXEC_INSERT:
		if ( !text || text[ 0 ] == '\0' )
			break;
		if ( m_text.empty() )
			m_text.assign( text );
		else
		{
			// Prepend `text` then a newline, then the existing buffer.
			std::string tmp;
			tmp.reserve( std::strlen( text ) + 1 + m_text.size() );
			tmp.assign( text );
			tmp.push_back( '\n' );
			tmp.append( m_text );
			m_text.swap( tmp );
		}
		break;
	case VE_CMD_EXEC_APPEND:
		if ( !text || text[ 0 ] == '\0' )
			break;
		m_text.append( text );
		break;
	default:
		derr_fatal( "veCmd::execute: bad veCmdExecWhen %d\n", static_cast< int >( when ) );
		break;
	}
}

// Process buffered text line-by-line, honoring `;` and newline separators
// (with quote/comment awareness), invoking `executeTokenized()` for each.
// If `m_wait > 0` at entry, it is decremented and the buffered remainder
// (deferred from a prior `wait`) is processed. Encountering a `wait`
// command sets `m_wait` and defers the remaining buffer to a later call.
void veCmd::execute()
{
	if ( m_wait > 0 )
		m_wait--;

	while ( !m_text.empty() )
	{
		// Scan for the first line terminator that isn't nested inside a
		// double-quoted region or a comment. Single quotes do NOT protect
		// against ';' (or '\n'/'\r') — they only group text for tokenisation,
		// not for command-buffer splitting. The ';' character always terminates
		// the current command at this layer.
		size_t i = 0;
		size_t end = std::string::npos;
		bool inDoubleQuote = false;
		int slashComment = 0;     // 0=no, 1=//, 2=/*
		while ( i < m_text.size() )
		{
			char c = m_text[ i ];
			if ( slashComment == 1 )
			{
				// //-comment: only a newline breaks it
				if ( c == '\n' || c == '\r' )
				{
					end = i;
					break;
				}
			}
			else if ( slashComment == 2 )
			{
				if ( c == '*' && i + 1 < m_text.size() && m_text[ i + 1 ] == '/' )
				{
					slashComment = 0;
					++i;
				}
			}
			else if ( inDoubleQuote )
			{
				if ( c == '"' )
					inDoubleQuote = false;
				// Inside double quotes: ';' and newlines are protected.
				// Single quotes inside double quotes are just characters.
			}
			else if ( c == '"' )
			{
				inDoubleQuote = true;
			}
			// Single quotes are transparent to the line splitter; they are
			// still recognised by the tokenizer as argument groupers.
			else if ( c == '/' && i + 1 < m_text.size()
				&& ( m_text[ i + 1 ] == '/' || m_text[ i + 1 ] == '*' ) )
			{
				slashComment = ( m_text[ i + 1 ] == '/' ) ? 1 : 2;
				++i;
			}
			else if ( c == ';' || c == '\n' || c == '\r' )
			{
				end = i;
				break;
			}
			++i;
		}

		if ( end == std::string::npos )
			end = m_text.size();

		m_line.assign( m_text, 0, end );
		// Consume the line and its terminator (if present).
		size_t consume = end;
		if ( consume < m_text.size() )
		{
			char c = m_text[ consume ];
			if ( c == '\r' && consume + 1 < m_text.size() && m_text[ consume + 1 ] == '\n' )
				consume += 2;
			else
				consume += 1;
		}
		m_text.erase( 0, consume );

		if ( !m_line.empty() )
		{
			tokenizeString( m_line.c_str() );
			if ( argc() > 0 )
				executeTokenized();
		}

		// A wait command just set m_wait; defer the rest of the buffer.
		if ( m_wait > 0 )
			return;
	}
}

int veCmd::argc( void )
{
	return static_cast< int >( m_argv.size() );
}

const char* veCmd::argv( int arg )
{
	if ( arg < 0 || arg >= static_cast< int >( m_argv.size() ) )
		return "";
	return m_tokens.data() + m_argv[ arg ];
}

std::string veCmd::args( void )
{
	return argsFrom( 1 );
}

std::string veCmd::argsFrom( int arg )
{
	if ( arg < 0 )
		arg = 0;
	std::string out;
	for ( int i = arg; i < static_cast< int >( m_argv.size() ); ++i )
	{
		if ( i > arg )
			out.push_back( ' ' );
		out.append( m_tokens.data() + m_argv[ i ] );
	}
	return out;
}

const char* veCmd::cmd( void )
{
	return m_cmd.c_str();
}

void veCmd::tokenizeString2( const char *text, bool ignoreQuotes )
{
	m_argv.clear();
	m_tokens.clear();
	if ( !text )
		return;
	m_cmd = text;

	auto startToken = [&]( size_t offset )
	{
		m_argv.push_back( static_cast< int >( offset ) );
	};

	const char* p = text;
	while ( *p )
	{
		// Skip whitespace.
		while ( *p && *p <= ' ' )
			++p;
		if ( !*p )
			break;
		if ( static_cast< int >( m_argv.size() ) >= VE_MAX_STRING_TOKENS )
			break;

		// Line comment // ...
		if ( p[ 0 ] == '/' && p[ 1 ] == '/' )
			break;

		// Block comment /* ... */
		if ( p[ 0 ] == '/' && p[ 1 ] == '*' )
		{
			p += 2;
			while ( *p && !( p[ 0 ] == '*' && p[ 1 ] == '/' ) )
				++p;
			if ( *p )
				p += 2;
			continue;
		}

		// Quoted token (single or double quote), only when not ignoreQuotes.
		if ( !ignoreQuotes && ( *p == '"' || *p == '\'' ) )
		{
			char quote = *p++;
			size_t start = m_tokens.size();
			startToken( start );
			while ( *p && *p != quote )
				m_tokens.push_back( *p++ );
			if ( *p == quote )
				++p;
			m_tokens.push_back( '\0' );
			continue;
		}

		// Regular token: copy while > ' ' and not the start of comments/quotes.
		size_t start = m_tokens.size();
		startToken( start );
		while ( *p )
		{
			char c = *p;
			if ( c <= ' ' )
				break;
			if ( !ignoreQuotes && c == '"' )
				break;
			if ( !ignoreQuotes && c == '\'' )
				break;
			if ( c == '/' && ( p[ 1 ] == '/' || p[ 1 ] == '*' ) )
				break;
			m_tokens.push_back( c );
			++p;
		}
		m_tokens.push_back( '\0' );
	}
}

void veCmd::tokenizeString( const char *text )
{
	tokenizeString2( text, false );
}

void veCmd::tokenizeStringIgnoreQuotes( const char *text )
{
	tokenizeString2( text, true );
}

// INSERT `text` into the buffer and run the buffered executor.
void veCmd::executeString( const char *text )
{
	if ( !text || text[ 0 ] == '\0' )
		return;
	execute( VE_CMD_EXEC_INSERT, text );
	execute();
}

// Dispatch the most recently tokenized command.
void veCmd::executeTokenized()
{
	const char* name = argv( 0 );
	if ( !name || name[ 0 ] == '\0' )
		return;

	// Aliases take priority.
	auto aliasIt = m_aliases.find( name );
	if ( aliasIt != m_aliases.end() )
	{
		execute( VE_CMD_EXEC_INSERT, aliasIt->second.c_str() );
		return;
	}

	// Registered command.
	auto cmdIt = m_functions.find( name );
	if ( cmdIt != m_functions.end() )
	{
		if ( cmdIt->second.func )
			cmdIt->second.func();
		return;
	}

	// Cvar command handler.
	if ( veCVar::command() )
		return;

	dinfo( "%s: unknown command.\n", name );
}

void veCmd::addCommand( const char *name, veCmdFunc function )
{
	if ( !name )
		return;
	if ( m_functions.find( name ) != m_functions.end() )
	{
		dinfo( "veCmd::addCommand(): %s already defined!\n", name );
		return;
	}
	veCmdFuncHandler h;
	h.name = name;
	h.func = std::move( function );
	m_functions.emplace( name, std::move( h ) );
}

void veCmd::removeCommand( const char *name )
{
	if ( !name )
		return;
	m_functions.erase( name );
}

void veCmd::commandCompletion( std::function< void( const char *s, const char *expr ) > callback )
{
	for ( const auto& kv : m_functions )
	{
		const char* expr = kv.second.complete.empty() ? nullptr : kv.second.complete.c_str();
		callback( kv.first.c_str(), expr );
	}
	for ( const auto& kv : m_aliases )
		callback( kv.first.c_str(), nullptr );
}

void veCmd::setCommandCompletion( const char *command, const std::string& complete )
{
	if ( !command )
		return;
	auto it = m_functions.find( command );
	if ( it == m_functions.end() )
	{
		veCmdFuncHandler h;
		h.name = command;
		h.complete = complete;
		m_functions.emplace( command, std::move( h ) );
	}
	else
	{
		it->second.complete = complete;
	}
}

// Case-(in)sensitive substring search. Empty needle matches at the haystack.
const char* veCmd::stringContains( const char *str1, const char *str2, int caseSensitive )
{
	if ( !str1 || !str2 )
		return nullptr;
	if ( *str2 == '\0' )
		return str1;
	if ( caseSensitive )
		return std::strstr( str1, str2 );

	for ( const char* p = str1; *p; ++p )
	{
		const char* a = p;
		const char* b = str2;
		while ( *a && *b && std::toupper( static_cast< unsigned char >( *a ) ) == std::toupper( static_cast< unsigned char >( *b ) ) )
		{
			++a;
			++b;
		}
		if ( !*b )
			return p;
	}
	return nullptr;
}

// Case-(in)sensitive glob filter supporting '*', '?', and character classes
// '[abc]' / '[a-z]'. '[[' is treated as a literal '['.
int veCmd::filter( const char *filter, const char *name, int caseSensitive )
{
	if ( !filter || !name )
		return 0;

	auto toUpper = [caseSensitive]( char c ) -> char
	{
		return caseSensitive ? c : static_cast< char >( std::toupper( static_cast< unsigned char >( c ) ) );
	};

	char buffer[ VE_MAX_TOKEN_LENGTH ];
	char* outPtr = buffer;
	const char* f = filter;
	const char* n = name;
	const char* fCheckpoint = nullptr;
	const char* nCheckpoint = nullptr;
	char* outCheckpoint = nullptr;

	while ( *f )
	{
		if ( static_cast< size_t >( outPtr - buffer ) >= sizeof( buffer ) - 1 )
		{
			derr( "Cmd::filter: command filter length too long (> %d).\n", VE_MAX_TOKEN_LENGTH );
			buffer[ sizeof( buffer ) - 1 ] = '\0';
			return 0;
		}

		if ( *f == '*' )
		{
			// Collapse runs of '*' and remember the position after them.
			while ( *f == '*' )
				++f;
			if ( !*f )
				return 1; // trailing '*' matches rest of name
			fCheckpoint = f;
			nCheckpoint = n;
			outCheckpoint = outPtr;
			continue;
		}
		if ( *f == '?' )
		{
			if ( *n == '\0' )
			{
				if ( !fCheckpoint || *( nCheckpoint + 1 ) == '\0' )
					return 0;
				f = fCheckpoint;
				n = ++nCheckpoint;
				outPtr = outCheckpoint;
				continue;
			}
			*outPtr++ = *n++;
			++f;
			continue;
		}
		if ( *f == '[' )
		{
			if ( f[ 1 ] == '[' )
			{
				if ( *n != '[' )
				{
					if ( !fCheckpoint || *( nCheckpoint + 1 ) == '\0' )
						return 0;
					f = fCheckpoint;
					n = ++nCheckpoint;
					outPtr = outCheckpoint;
					continue;
				}
				*outPtr++ = '[';
				++n;
				f += 2;
				continue;
			}
			bool negate = false;
			++f;
			if ( *f == '!' || *f == '^' )
			{
				negate = true;
				++f;
			}
			bool matched = false;
			while ( *f && *f != ']' )
			{
				char lo = toUpper( *f );
				if ( f[ 1 ] == '-' && f[ 2 ] && f[ 2 ] != ']' )
				{
					char hi = toUpper( f[ 2 ] );
					char nc = toUpper( *n );
					if ( nc >= lo && nc <= hi )
						matched = true;
					f += 3;
				}
				else
				{
					if ( *n && toUpper( *n ) == lo )
						matched = true;
					++f;
				}
			}
			if ( *f == ']' )
				++f;
			if ( *n && matched != negate )
			{
				*outPtr++ = *n++;
			}
			else
			{
				if ( !fCheckpoint || *( nCheckpoint + 1 ) == '\0' )
					return 0;
				f = fCheckpoint;
				n = ++nCheckpoint;
				outPtr = outCheckpoint;
				continue;
			}
			continue;
		}
		// Literal char.
		if ( *n == '\0' || toUpper( *f ) != toUpper( *n ) )
		{
			if ( !fCheckpoint || *( nCheckpoint + 1 ) == '\0' )
				return 0;
			f = fCheckpoint;
			n = ++nCheckpoint;
			outPtr = outCheckpoint;
			continue;
		}
		*outPtr++ = *n++;
		++f;
	}

	// Pattern consumed: name must be fully consumed too.
	*outPtr = '\0';
	return *n == '\0' ? 1 : 0;
}

// ---------------------------------- Default Console Command Functions --------------------------

// Causes execution of the remainder of the command buffer to be delayed until
// next frame.  This allows commands like:
// bind g "cmd use rocket ; +attack ; wait ; -attack ; cmd use blaster"
//
void veCmd_WaitFunc( void )
{
	auto& c = veGetCmd();
	int wait = 1;
	if ( c.argc() == 2 )
	{
		wait = std::atoi( c.argv( 1 ) );
		if ( wait < 0 )
			wait = 1;
	}
	c.setWait( wait );
}

void veCmd_EchoFunc()
{
	auto& c = veGetCmd();
	dinfo( "%s\n", c.args().c_str() );
}

void veCmd_AliasFunc()
{
	auto& c = veGetCmd();
	if ( c.argc() == 1 )
	{
		for ( const auto& kv : c.getAliases() )
			dinfo( "%s : %s\n", kv.first.c_str(), kv.second.c_str() );
		dinfo( "%d aliases\n", static_cast< int >( c.getAliases().size() ) );
		return;
	}

	const char* name = c.argv( 1 );
	if ( !veIsValidConsoleToken( name ) || std::strcmp( name, "alias" ) == 0 )
	{
		dinfo( "alias: invalid alias name '%s'\n", name ? name : "" );
		return;
	}

	if ( c.argc() == 2 )
	{
		auto it = c.getAliases().find( name );
		if ( it != c.getAliases().end() )
			dinfo( "%s : %s\n", it->first.c_str(), it->second.c_str() );
		else
			dinfo( "alias %s does not exist.\n", name );
		return;
	}

	c.getAliases()[ name ] = c.argsFrom( 2 );
}

void veCmd_ExecFunc()
{
	auto& c = veGetCmd();
	if ( c.argc() != 2 )
	{
		dinfo( "usage: exec <filename>\n" );
		return;
	}
	const char* filename = c.argv( 1 );
	std::ifstream f( filename, std::ios::binary );
	if ( !f.good() )
	{
		dinfo( "couldn't exec %s\n", filename );
		return;
	}
	std::ostringstream ss;
	ss << f.rdbuf();
	std::string contents = ss.str();
	if ( contents.empty() )
		return;
	c.execute( VE_CMD_EXEC_INSERT, contents.c_str() );
}

void veCmd_ListFunc()
{
	auto& c = veGetCmd();
	const char* match = c.argc() > 1 ? c.argv( 1 ) : nullptr;
	int total = 0;
	for ( const auto& kv : c.getFunctionsList() )
	{
		const char* name = kv.second.name.c_str();
		if ( match && !veCmd::filter( match, name, false ) )
			continue;
		dinfo( "%s\n", name );
		++total;
	}
	// Total reported count reflects every command in the list, not the filtered count.
	dinfo( "%d commands\n", static_cast< int >( c.getFunctionsList().size() ) );
	(void)total;
}

void veCVarCmd_PrintFunc()
{
	auto& c = veGetCmd();
	if ( c.argc() != 2 )
	{
		dinfo( "usage: print <variable>\n" );
		return;
	}
	const char* name = c.argv( 1 );
	veCVar* v = veCVar::find( name );
	if ( v )
		veCVar::print( v );
	else
		dinfo( "Cvar %s does not exist.\n", name );
}

void veCVarCmd_ToggleFunc()
{
	auto& c = veGetCmd();
	if ( c.argc() < 2 )
	{
		dinfo( "usage: toggle <variable> [value1, value2, ...]\n" );
		return;
	}
	const char* name = c.argv( 1 );
	if ( c.argc() == 2 )
	{
		char buf[ 32 ];
		std::snprintf( buf, sizeof( buf ), "%d", !static_cast< int >( veCVar::variableValue( name ) ) );
		veCVar::set2( name, buf, false );
		return;
	}
	if ( c.argc() == 3 )
	{
		dinfo( "toggle: nothing to toggle to\n" );
		return;
	}
	// argc > 3: cycle through argv(2)..argv(argc-2), wrapping to argv(2).
	const char* cur = veCVar::variableString( name );
	int nextIdx = 2;
	for ( int i = 2; i < c.argc() - 1; ++i )
	{
		if ( std::strcmp( cur, c.argv( i ) ) == 0 )
		{
			nextIdx = i + 1;
			break;
		}
	}
	if ( nextIdx >= c.argc() )
		nextIdx = 2;
	veCVar::set2( name, c.argv( nextIdx ), false );
}

void veCVarCmd_SetFunc()
{
	auto& c = veGetCmd();
	int ac = c.argc();
	const char* cmd = c.argv( 0 );
	if ( ac < 2 )
	{
		dinfo( "usage: %s <variable> <value>\n", cmd ? cmd : "" );
		return;
	}
	if ( ac == 2 )
	{
		veCVarCmd_PrintFunc();
		return;
	}
	veCVar* v = veCVar::set2( c.argv( 1 ), c.argsFrom( 2 ).c_str(), false );
	if ( !v )
		return;
	if ( cmd && cmd[ 3 ] )
	{
		int extra = 0;
		switch ( cmd[ 3 ] )
		{
		case 'a': extra = VE_CVAR_ARCHIVE; break;
		case 'u': extra = VE_CVAR_USERINFO; break;
		case 's': extra = VE_CVAR_SERVERINFO; break;
		default: break;
		}
		if ( extra )
		{
			v->getFlags() |= extra;
			veCVar::getModifiedFlags() |= extra;
		}
	}
}

void veCVarCmd_ResetFunc()
{
	auto& c = veGetCmd();
	if ( c.argc() != 2 )
	{
		dinfo( "usage: reset <variable>\n" );
		return;
	}
	veCVar::reset( c.argv( 1 ) );
}

void veCVarCmd_ListFunc()
{
	auto& c = veGetCmd();
	const char* match = c.argc() > 1 ? c.argv( 1 ) : nullptr;
	veCVar::list( match );
}

void veCVarCmd_ListModifiedFunc()
{
	auto& c = veGetCmd();
	const char* match = c.argc() > 1 ? c.argv( 1 ) : nullptr;
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
	c.setCommandCompletion( "set", "C_V_" );
	c.setCommandCompletion( "sets", "C_V_" );
	c.setCommandCompletion( "setu", "C_V_" );
	c.setCommandCompletion( "seta", "C_V_" );

	c.addCommand( "reset", veCVarCmd_ResetFunc );
	c.setCommandCompletion( "reset", "C_V_" );

	c.addCommand( "cvarlist", veCVarCmd_ListFunc );
	c.addCommand( "cvar_list", veCVarCmd_ListFunc );

	c.addCommand( "cvar_modified", veCVarCmd_ListModifiedFunc );

	c.addCommand( "cvar_restart", veCVarCmd_RestartFunc );
}

void veCmd_InitDefaultFunctions()
{
	static bool initialised = false;
	if ( initialised )
		return;
	initialised = true;
	auto& c = veGetCmd();
	c.addCommand( "cmdlist", veCmd_ListFunc );
	c.addCommand( "echo", veCmd_EchoFunc );
	c.addCommand( "exec", veCmd_ExecFunc );
	c.addCommand( "wait", veCmd_WaitFunc );
	veCVar_InitCmd();
}

// ------------------------------------------------- Startup Command Extraction -------------------------------------------------

void veExtractStartupCommands( int argc, const char* const argv[],
                                std::vector< std::string >& outRemainingArgv,
                                std::vector< std::string >& outCommands )
{
	outRemainingArgv.clear();
	outCommands.clear();
	if ( argc <= 0 || !argv )
		return;

	for ( int i = 0; i < argc; ++i )
	{
		const char* tok = argv[ i ];
		if ( !tok )
			continue;
		if ( tok[ 0 ] == '+' && tok[ 1 ] != '\0' )
		{
			// Begin collecting a new command string from this token's payload.
			std::string cmd( tok + 1 );
			++i;
			while ( i < argc && argv[ i ] && argv[ i ][ 0 ] != '+' )
			{
				cmd.push_back( ' ' );
				cmd.append( argv[ i ] );
				++i;
			}
			outCommands.push_back( std::move( cmd ) );
			--i; // rewind so the outer for-loop's ++i lands on the next unconsumed token
		}
		else
		{
			outRemainingArgv.emplace_back( tok );
		}
	}
}

void veExecuteStartupCommands( const std::vector< std::string >& commands )
{
	for ( const std::string& cmd : commands )
		veGetCmd().execute( VE_CMD_EXEC_NOW, cmd.c_str() );
}

// ------------------------------------------------- Misc. Other Utils -------------------------------------------------

// Printf into a rotating ring of thread-local temp buffers. Each call
// selects the next slot (round-robin) and formats into it bounded by the
// buffer size. With N slots, up to N concurrent results are simultaneously
// valid; the (N+1)th call reuses the first buffer.
char* veq3_va( const char *format, ... )
{
	constexpr int NUM_BUFFERS = 4;
	constexpr int BUFFER_SIZE = 8192;
	thread_local std::array< std::array< char, BUFFER_SIZE >, NUM_BUFFERS > buffers{};
	thread_local int nextBuffer = 0;

	char* out = buffers[ nextBuffer ].data();
	nextBuffer = ( nextBuffer + 1 ) % NUM_BUFFERS;

	va_list args;
	va_start( args, format );
	std::vsnprintf( out, BUFFER_SIZE, format, args );
	va_end( args );

	return out;
}
