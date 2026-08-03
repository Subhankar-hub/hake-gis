/***************************************************************************
    mainwin.cpp
    ---------------------
    begin                : February 2017
    copyright            : (C) 2017 by Juergen E. Fischer
    email                : jef at norbit dot de
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <fstream>
#include <io.h>
#include <iostream>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

namespace
{

void showError( std::string message, std::string title )
{
  std::string newmessage = "Oops, looks like an error loading Hake GIS \n\n Details: \n\n" + message;
  MessageBoxA( nullptr, newmessage.c_str(), title.c_str(), MB_ICONERROR | MB_OK );
  std::cerr << message << std::endl;
}

std::string moduleExePath()
{
  DWORD l = MAX_PATH;
  std::unique_ptr<char[]> filepath;
  for ( ;; )
  {
    filepath.reset( new char[l] );
    if ( GetModuleFileNameA( nullptr, filepath.get(), l ) < l )
      break;

    l += MAX_PATH;
  }
  return std::string( filepath.get() );
}

std::string dirnameOf( const std::string &path )
{
  const size_t pos = path.find_last_of( "\\/" );
  if ( pos == std::string::npos )
    return std::string( "." );
  if ( pos == 0 )
    return path.substr( 0, 1 );
  return path.substr( 0, pos );
}

std::string parentDir( const std::string &path )
{
  return dirnameOf( path );
}

void replaceAll( std::string &haystack, const std::string &from, const std::string &to )
{
  if ( from.empty() )
    return;
  size_t start = 0;
  while ( ( start = haystack.find( from, start ) ) != std::string::npos )
  {
    haystack.replace( start, from.length(), to );
    start += to.length();
  }
}

std::string expandEnvStrings( const std::string &value )
{
  DWORD needed = ExpandEnvironmentStringsA( value.c_str(), nullptr, 0 );
  if ( needed == 0 )
    return value;
  std::vector<char> buf( needed );
  if ( ExpandEnvironmentStringsA( value.c_str(), buf.data(), needed ) == 0 )
    return value;
  return std::string( buf.data() );
}

std::wstring utf8ToWide( const std::string &s )
{
  if ( s.empty() )
    return std::wstring();
  const int n = MultiByteToWideChar( CP_UTF8, 0, s.c_str(), -1, nullptr, 0 );
  if ( n <= 0 )
    return std::wstring();
  std::wstring out( static_cast<size_t>( n - 1 ), L'\0' );
  MultiByteToWideChar( CP_UTF8, 0, s.c_str(), -1, &out[0], n );
  return out;
}

bool putEnvVar( const std::string &assignment )
{
  // _putenv requires "NAME=value". Windows process env block is limited;
  // a PATH that embeds the full machine PATH often fails here.
  return _putenv( assignment.c_str() ) == 0;
}

bool applyEnvLine( const std::string &rawLine, const std::string &appDir, const std::string &prefixDir )
{
  std::string line = rawLine;
  // Trim CR (Windows newlines) and skip blanks / comments
  if ( !line.empty() && line.back() == '\r' )
    line.pop_back();
  if ( line.empty() || line[0] == '#' )
    return true;

  const size_t eq = line.find( '=' );
  if ( eq == std::string::npos )
    return true;

  std::string name = line.substr( 0, eq );
  std::string value = line.substr( eq + 1 );
  replaceAll( value, "{app}", appDir );
  replaceAll( value, "{prefix}", prefixDir );
  value = expandEnvStrings( value );

  // Keep PATH short: prepend our bin dir, do not require shipping the whole system PATH.
  if ( _stricmp( name.c_str(), "PATH" ) == 0 )
  {
    const char *oldPath = getenv( "PATH" );
    std::string merged = value;
    if ( oldPath && *oldPath )
    {
      // Prefer our directories first so DLL resolution finds install\bin without a huge .env PATH.
      merged = value + ";" + oldPath;
    }
    // If the merged PATH is enormous, fall back to install bin only + system32 essentials.
    if ( merged.size() > 30000 )
    {
      char windir[MAX_PATH] = {};
      GetWindowsDirectoryA( windir, MAX_PATH );
      merged = value + ";" + windir + ";" + std::string( windir ) + "\\system32;" + std::string( windir ) + "\\system32\\WBem";
    }
    return putEnvVar( name + "=" + merged );
  }

  return putEnvVar( name + "=" + value );
}

std::vector<std::string> defaultEnvLines( const std::string &appDir, const std::string &prefixDir )
{
  return {
    "PATH=" + appDir,
    "QGIS_PREFIX_PATH=" + prefixDir,
    "PROJ_DATA=" + prefixDir + "\\share\\proj",
    "GDAL_DATA=" + prefixDir + "\\share\\gdal",
    "QT_PLUGIN_PATH=" + appDir + "\\Qt6\\plugins;" + appDir + "\\Qt6\\plugins\\crypto",
    "PYTHONHOME=" + appDir,
    "PYTHONPATH=" + prefixDir + "\\python;" + appDir + "\\Lib;" + appDir + "\\Lib\\site-packages;" + appDir + "\\DLLs",
  };
}

void addDllSearchDir( const std::string &dir,
                      BOOL ( *SetDefaultDllDirectories )( DWORD ),
                      DLL_DIRECTORY_COOKIE ( *AddDllDirectory )( PCWSTR ) )
{
  if ( !SetDefaultDllDirectories || !AddDllDirectory )
    return;
  const std::wstring wdir = utf8ToWide( dir );
  if ( !wdir.empty() )
    AddDllDirectory( wdir.c_str() );
}

} // namespace

int CALLBACK WinMain( HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/ )
{
  const std::string exename( moduleExePath() );
  const std::string basename( exename.substr( 0, exename.size() - 4 ) );
  const std::string appDir = dirnameOf( exename );
  const std::string prefixDir = parentDir( appDir );

  if ( getenv( "OSGEO4W_ROOT" ) && __argc == 2 && strcmp( __argv[1], "--postinstall" ) == 0 )
  {
    std::string envfile( basename + ".env" );

    // write or update environment file
    if ( _access( envfile.c_str(), 0 ) < 0 || _access( envfile.c_str(), 2 ) == 0 )
    {
      std::list<std::string> vars;

      try
      {
        std::ifstream varfile;
        varfile.open( basename + ".vars" );

        std::string var;
        while ( std::getline( varfile, var ) )
        {
          vars.push_back( var );
        }

        varfile.close();
      }
      catch ( std::ifstream::failure &e )
      {
        std::string message = "Could not read environment variable list " + basename + ".vars" + " [" + e.what() + "]";
        showError( message, "Error loading Hake GIS" );
        return EXIT_FAILURE;
      }

      try
      {
        std::ofstream file;
        file.open( envfile, std::ifstream::out );

        for ( std::list<std::string>::const_iterator it = vars.begin(); it != vars.end(); ++it )
        {
          if ( getenv( it->c_str() ) )
            file << *it << "=" << getenv( it->c_str() ) << std::endl;
        }
      }
      catch ( std::ifstream::failure &e )
      {
        std::string message = "Could not write environment file " + basename + ".env" + " [" + e.what() + "]";
        showError( message, "Error loading Hake GIS" );
        return EXIT_FAILURE;
      }
    }

    return EXIT_SUCCESS;
  }

  // Prefer packaged .env; if missing or unreadable, synthesize a short install-local env.
  {
    std::ifstream file( basename + ".env" );
    bool appliedAny = false;
    if ( file )
    {
      std::string var;
      while ( std::getline( file, var ) )
      {
        if ( !applyEnvLine( var, appDir, prefixDir ) )
        {
          // PATH overflow / env-block full: fall back to a minimal PATH and continue.
          if ( var.rfind( "PATH=", 0 ) == 0 || var.rfind( "path=", 0 ) == 0 )
          {
            putEnvVar( "PATH=" + appDir );
            continue;
          }
          std::string message = "Could not set environment variable (environment block may be full):\n" + var
                                + "\n\nHelp: shorten the Windows system PATH, or edit:\n" + basename + ".env";
          showError( message, "Error loading Hake GIS" );
          return EXIT_FAILURE;
        }
        appliedAny = true;
      }
    }
    if ( !appliedAny )
    {
      const auto defaults = defaultEnvLines( appDir, prefixDir );
      for ( const std::string &line : defaults )
      {
        if ( !applyEnvLine( line, appDir, prefixDir ) )
        {
          putEnvVar( "PATH=" + appDir );
        }
      }
    }
  }

#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
  HINSTANCE hKernelDLL = LoadLibraryA( "kernel32.dll" );
  BOOL ( *SetDefaultDllDirectories )( DWORD ) = hKernelDLL ? reinterpret_cast<BOOL ( * )( DWORD )>( GetProcAddress( hKernelDLL, "SetDefaultDllDirectories" ) ) : nullptr;
  DLL_DIRECTORY_COOKIE ( *AddDllDirectory )( PCWSTR ) = hKernelDLL ? reinterpret_cast<DLL_DIRECTORY_COOKIE ( * )( PCWSTR )>( GetProcAddress( hKernelDLL, "AddDllDirectory" ) ) : nullptr;
#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic pop
#endif

  if ( SetDefaultDllDirectories && AddDllDirectory )
  {
    SetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_DEFAULT_DIRS );

    // Always search the install bin directory first (independent of PATH length).
    addDllSearchDir( appDir, SetDefaultDllDirectories, AddDllDirectory );

    wchar_t windir[MAX_PATH];
    GetWindowsDirectoryW( windir, MAX_PATH );
    wchar_t systemdir[MAX_PATH];
    GetSystemDirectoryW( systemdir, MAX_PATH );

    wchar_t *path = _wgetenv( L"PATH" ) ? wcsdup( _wgetenv( L"PATH" ) ) : nullptr;
    if ( path )
    {
#ifdef _UCRT
      for ( wchar_t *p = wcstok( path, L";", nullptr ); p; p = wcstok( nullptr, L";", nullptr ) )
#else
      for ( wchar_t *p = wcstok( path, L";" ); p; p = wcstok( nullptr, L";" ) )
#endif
      {
        if ( _wcsicmp( p, windir ) == 0 )
          continue;
        if ( _wcsicmp( p, systemdir ) == 0 )
          continue;
        AddDllDirectory( p );
      }
      free( path );
    }
  }

#ifndef QGIS_APP_DLL_NAME
#ifdef _MSC_VER
#define QGIS_APP_DLL_NAME "qgis_app.dll"
#else
// MinGW
#define QGIS_APP_DLL_NAME "libqgis_app.dll"
#endif
#endif

  HINSTANCE hGetProcIDDLL = LoadLibraryA( QGIS_APP_DLL_NAME );

  if ( !hGetProcIDDLL )
  {
    DWORD error = GetLastError();
    LPTSTR errorText = nullptr;

    FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), ( LPTSTR ) &errorText, 0, nullptr );

    std::string message = std::string( "Could not load " ) + QGIS_APP_DLL_NAME + " \n Windows Error: " + std::string( errorText ? errorText : "" )
                          + "\n Help: \n\n Check " + basename + ".env for correct environment paths"
                          + "\n Ensure " + QGIS_APP_DLL_NAME + " exists in:\n " + appDir;
    showError( message, "Error loading Hake GIS" );

    LocalFree( errorText );
    return EXIT_FAILURE;
  }

#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
  int ( *realmain )( int, char *[] ) = ( int ( * )( int, char *[] ) ) GetProcAddress( hGetProcIDDLL, "main" );
#ifndef _MSC_VER // MinGW
#pragma GCC diagnostic pop
#endif

  if ( !realmain )
  {
    showError( std::string( "Could not locate main function in " ) + QGIS_APP_DLL_NAME, "Error loading Hake GIS" );
    return EXIT_FAILURE;
  }

  return realmain( __argc, __argv );
}
