/*
 *  Copyright � 2006-2012 SplinterGU (Fenix/Bennugd)
 *  Copyright � 2002-2006 Fenix Team (Fenix)
 *  Copyright � 1999-2002 Jos� Luis Cebri�n Pag�e (Fenix)
 *
 *  This file is part of Bennu - Game Development
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty. In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 *
 */

/*
 * INCLUDES
 */

#ifdef _WIN32
#define  _WIN32_WINNT 0x0500
#include "sorrnet.h"
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bgdi.h"
#include "bgdrtm.h"
#include "xstrings.h"
#include "dirs.h"

#include <SDL.h>
#if defined(TARGET_IOS)
#include <SDL.h>
#elif defined(TARGET_WII)
#include <SDL.h>
#include <fat.h>
#elif defined(TARGET_PSP)
#include "psp.h"
#endif

/* ---------------------------------------------------------------------- */

static char * dcb_exts[] = { ".dcb", ".dat", ".bin", NULL };

static int standalone  = 0;  /* 1 only if this is an standalone interpreter   */
static int embedded    = 0;  /* 1 only if this is a stub with an embedded DCB */

/* ---------------------------------------------------------------------- */

/*
 *  FUNCTION : main
 *
 *  Main function
 *
 *  PARAMS:
 *      INT n: ERROR LEVEL to return to OS
 *
 *  RETURN VALUE:
 *      No value
 *
 */


int _newlib_heap_size_user   = 168 * 1024 * 1024;

#if defined(TARGET_ANDROID)
#include <android/log.h>
#include <signal.h>
#include <dlfcn.h>
#include <ucontext.h>
#include <stdint.h>

static void crash_print_addr( const char * tag, uintptr_t a )
{
    Dl_info di;
    if ( a && dladdr( ( void * ) a, &di ) && di.dli_fname )
    {
        const char * lib = strrchr( di.dli_fname, '/' );
        fprintf( stderr, "  %s %p  %s+0x%lx  (%s)\n", tag, ( void * ) a, lib ? lib + 1 : di.dli_fname,
                 ( unsigned long )( a - ( uintptr_t ) di.dli_fbase ), di.dli_sname ? di.dli_sname : "?" );
    }
    else
        fprintf( stderr, "  %s %p\n", tag, ( void * ) a );
}

static void crash_handler( int sig, siginfo_t * si, void * ctx )
{
    ucontext_t * uc = ( ucontext_t * ) ctx;
    fprintf( stderr, "\n*** CRASH: signal %d, fault address %p ***\n", sig, si ? si->si_addr : NULL );
#if defined(__aarch64__)
    {
        uintptr_t pc = uc->uc_mcontext.pc, lr = uc->uc_mcontext.regs[30], fp = uc->uc_mcontext.regs[29];
        int i;
        crash_print_addr( "pc", pc );
        crash_print_addr( "lr", lr );
        for ( i = 0; i < 24 && fp && ( fp & 7 ) == 0; i++ )
        {
            uintptr_t * f = ( uintptr_t * ) fp;
            uintptr_t ret = f[1];
            if ( !ret ) break;
            crash_print_addr( "bt", ret );
            if ( f[0] <= fp ) break;
            fp = f[0];
        }
        for ( i = 0; i < 31; i++ ) fprintf( stderr, "  x%d=%lx%s", i, ( unsigned long ) uc->uc_mcontext.regs[i], ( i % 4 == 3 ) ? "\n" : "" );
        fprintf( stderr, "\n" );
    }
#elif defined(__x86_64__)
    crash_print_addr( "pc", ( uintptr_t ) uc->uc_mcontext.gregs[REG_RIP] );
#endif
    fflush( stderr );
    signal( sig, SIG_DFL );
    raise( sig );
}

static void install_crash_handler( void )
{
    struct sigaction sa;
    static char altstack[ 65536 ];
    stack_t ss;
    ss.ss_sp = altstack; ss.ss_size = sizeof( altstack ); ss.ss_flags = 0;
    sigaltstack( &ss, NULL );
    memset( &sa, 0, sizeof( sa ) );
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction( SIGSEGV, &sa, NULL );
    sigaction( SIGBUS, &sa, NULL );
    sigaction( SIGFPE, &sa, NULL );
    sigaction( SIGILL, &sa, NULL );
    sigaction( SIGABRT, &sa, NULL );
}
#define STEP(x) SDL_Log( "step: %s", x )
#else
#define STEP(x)
#endif
#if defined(TARGET_ANDROID)
static void android_log_to_file( void *ud, int category, SDL_LogPriority prio, const char *msg )
{
    (void)ud; (void)category;
    fprintf( stderr, "%s\n", msg );
    __android_log_print( prio >= SDL_LOG_PRIORITY_ERROR ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, "SorR", "%s", msg );
}
#endif

int main( int argc, char *argv[] )
{
#ifdef TARGET_VITA
  scePowerSetArmClockFrequency(444);
  scePowerSetGpuClockFrequency(222);
  SDL_setenv("VITA_DISABLE_TOUCH_FRONT", "1");
  SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1");
#endif

    char * filename = NULL, dcbname[ __MAX_PATH ], *ptr ;
    int i, j, ret = -1;
    file * fp = NULL;
    INSTANCE * mainproc_running;
    dcb_signature dcb_signature;

#ifdef BGD_LOWMEM
    /* Route SDL / SDL_mixer allocations to the low (<2GB) arena too */
    {
        extern void *lowmem_malloc(size_t), *lowmem_calloc(size_t, size_t), *lowmem_realloc(void *, size_t);
        extern void lowmem_free(void *);
        SDL_SetMemoryFunctions(lowmem_malloc, lowmem_calloc, lowmem_realloc, lowmem_free);
    }
#endif

#if defined(TARGET_ANDROID)
    /* Android (SDLActivity): argv[0] = "app_process", argv[1] = game data dir,
       argv[2..] = normal bgdi arguments (e.g. "SorR.dat") */
    if ( argc < 3 )
    {
        SDL_Log( "bgdi: usage: <gamedir> <file.dat>" );
        return -1;
    }
    if ( chdir( argv[1] ) != 0 )
    {
        SDL_Log( "bgdi: cannot chdir to %s", argv[1] );
        return -1;
    }
    /* Log everything to <gamedir>/sorr_log.txt so problems can be diagnosed on device */
    if ( freopen( "sorr_log.txt", "w", stderr ) )
    {
        setvbuf( stderr, NULL, _IONBF, 0 );
        SDL_LogSetOutputFunction( android_log_to_file, NULL );
    }
    install_crash_handler();
    SDL_Log( "SorR Android: gamedir=%s dat=%s", argv[1], argv[2] );
    appexename = strdup( "bgdi" );
    appexefullpath = strdup( argv[1] );
    appexepath = calloc( 1, strlen( argv[1] ) + 2 );
    strcpy( appexepath, argv[1] );
    if ( appexepath[ strlen( appexepath ) - 1 ] != '/' ) strcat( appexepath, "/" );
    appname = strdup( "SorR" );
    /* drop the game dir argument */
    memmove( &argv[1], &argv[2], sizeof( char * ) * ( argc - 2 ) );
    argc-- ;
    argv[argc] = NULL;
#elif !defined(TARGET_VITA)
    /* get my executable name */
    ptr = argv[0] + strlen( argv[0] );
    while ( ptr > argv[0] && ptr[-1] != '\\' && ptr[-1] != '/' ) ptr-- ;
    appexename = strdup( ptr );

    /* get executable full pathname  */
    appexefullpath = getfullpath( argv[0] );
    if ( ( !strchr( argv[0], '\\' ) && !strchr( argv[0], '/' ) ) && !file_exists( appexefullpath ) )
    {
        char *p = whereis( appexename );
        if ( p )
        {
            char * tmp = calloc( 1, strlen( p ) + strlen( appexename ) + 2 );
            free( appexefullpath );
            sprintf( tmp, "%s/%s", p, appexename );
            appexefullpath = getfullpath( tmp );
            free( tmp );
        }
    }

    /* get pathname of executable */
    ptr = strstr( appexefullpath, appexename );
    appexepath = calloc( 1, ptr - appexefullpath + 1 );
    strncpy( appexepath, appexefullpath, ptr - appexefullpath );


#ifdef _WIN32
    appname = calloc( 1, strlen( appexename ) - 3 );
    strncpy( appname, appexename, strlen( appexename ) - 4 );
#else
    appname = strdup( appexename );
#endif

#endif // TARGET_VITA

    /* Juego en red (SORR_NET): el invitado no carga el juego, solo recibe imagen y sonido */
    sorrnet_init();
    if ( sorrnet_is_guest() ) { extern void sorr_signal_exit( void ); int r = sorrnet_guest_main(); sorr_signal_exit(); return r; }

#ifdef TARGET_PSP
    PSP_HEAP_SIZE_MAX ();
    pspDebugScreenInit();
    SetupCallbacks();
#endif

#ifdef TARGET_PSP
    standalone = ( strncmpi( appexename, "EBOOT", 4 ) == 0 ) ;
#else
#ifndef TARGET_VITA
    standalone = ( strncmpi( appexename, "bgdi", 4 ) == 0 ) ;
#endif
#endif

#ifdef TARGET_VITA
    appexepath = calloc( 1, 7 );
    strncpy( appexepath, "app0:/" , 7 );
    standalone = 1;
#endif

#ifdef TARGET_VITA
#else
    /* add binary path */
    file_addp( appexepath );
#endif

#ifdef TARGET_WII
    // Initialize the Wii FAT filesystem, check stuff
    if (!fatInitDefault()) {
        printf("Sorry, I cannot access the FAT filesystem on your card :(\n");
        exit(1);
    }
#endif

    if ( !standalone )
    {
        /* Hand-made interpreter: search for DCB at EOF */
        fp = file_open( argv[0], "rb0" );
        if ( fp )
        {
            file_seek( fp, -( int )sizeof( dcb_signature ), SEEK_END );
            file_read( fp, &dcb_signature, sizeof( dcb_signature ) );

            if ( strcmp( dcb_signature.magic, DCB_STUB_MAGIC ) == 0 )
            {
                ARRANGE_DWORD( &dcb_signature.dcb_offset );
                embedded = 1;
            }
        }

        filename = appname;
    }

    if ( standalone )
    {
        /* Calling BGDI.EXE so we must get all command line params */
#ifdef TARGET_PSP
    	filename="EBOOT.dcb";
    	glob_t globbuf;
    	glob("*.dcb", 0, NULL, &globbuf);
    	if (globbuf.gl_pathc>0){
    		filename=globbuf.gl_pathv[0];
    		filename[strlen(filename)-4]=0;
    	}
#endif

#ifdef TARGET_VITA
    file_addp( "ux0:/data/sorr/" );
    file_addp( "app0:/" );
    filename = "sorr.dat";
#endif



#ifndef TARGET_VITA
        for ( i = 1 ; i < argc ; i++ )
        {
            if ( argv[i][0] == '-' )
            {
                j = 1 ;
                while ( argv[i][j] )
                {
                    if ( argv[i][j] == 'd' ) debug = 1 ;
                    if ( argv[i][j] == 'i' )
                    {
                        if ( argv[i][j+1] == 0 )
                        {
                            if ( i == argc - 1 )
                            {
                                fprintf( stderr, "You must provide a directory" ) ;
                                exit( 0 );
                            }
                            file_addp( argv[i+1] );
                            i++ ;
                            break ;
                        }
                        file_addp( &argv[i][j + 1] ) ;
                        break ;
                    }
                    j++ ;
                }
            }
            else
            {
                if ( !filename )
                {
                    filename = argv[i] ;
                    if ( i < argc - 1 ) memmove( &argv[i], &argv[i+1], sizeof( char* ) * ( argc - i - 1 ) ) ;
                    argc-- ;
                    i-- ;
                }
            }
        }
#endif
        if ( !filename )
        {
            printf( BGDI_VERSION "\n"
                    "Copyright (c) 2006-2012 SplinterGU (Fenix/BennuGD)\n"
                    "Copyright (c) 2002-2006 Fenix Team (Fenix)\n"
                    "Copyright (c) 1999-2002 José Luis Cebrián Pagüe (Fenix)\n"
                    "Bennu Game Development comes with ABSOLUTELY NO WARRANTY;\n"
                    "see COPYING for details\n\n"
                    "Usage: %s [options] <data code block file>[.dcb]\n\n"
                    "   -d       Activate DEBUG mode\n"
                    "   -i dir   Adds the directory to the PATH\n"
                    "\n"
                    "This software is provided 'as-is', without any express or implied\n"
                    "warranty. In no event will the authors be held liable for any damages\n"
                    "arising from the use of this software.\n"
                    "\n"
                    "Permission is granted to anyone to use this software for any purpose,\n"
                    "including commercial applications, and to alter it and redistribute it\n"
                    "freely, subject to the following restrictions:\n"
                    "\n"
                    "   1. The origin of this software must not be misrepresented; you must not\n"
                    "   claim that you wrote the original software. If you use this software\n"
                    "   in a product, an acknowledgment in the product documentation would be\n"
                    "   appreciated but is not required.\n"
                    "\n"
                    "   2. Altered source versions must be plainly marked as such, and must not be\n"
                    "   misrepresented as being the original software.\n"
                    "\n"
                    "   3. This notice may not be removed or altered from any source\n"
                    "   distribution.\n",
                    argv[0] ) ;
            return -1 ;
        }
    }

    /* Initialization (modules needed before dcb_load) */

    STEP( "string_init" );
    string_init() ;
    init_c_type() ;
    STEP( "dcb_load" );

    /* Init application title for windowed modes */

    strcpy( dcbname, filename ) ;

    if ( appname && filename != appname )
    {
        free( appname );
        appname = strdup( filename ) ;
    }

    if ( !embedded )
    {
        /* First try to load directly (we expect myfile.dcb) */
        if ( !dcb_load( dcbname ) )
        {
            char ** dcbext = dcb_exts;
            int dcbloaded = 0;

            while ( dcbext && *dcbext )
            {
                strcpy( dcbname, filename ) ;
                strcat( dcbname, *dcbext ) ;
                if (( dcbloaded = dcb_load( dcbname ) ) ) break;
                dcbext++;
            }

            if ( !dcbloaded )
            {
                printf( "%s: doesn't exist or isn't version %d DCB compatible\n", filename, DCB_VERSION >> 8 ) ;
                return -1 ;
            }
        }
    }
    else
    {
        dcb_load_from( fp, dcbname, dcb_signature.dcb_offset );
    }

    /* If the dcb is not in debug mode */

    if ( dcb.data.NSourceFiles == 0 ) debug = 0;

    /* Initialization (modules needed after dcb_load) */

    STEP( "sysproc_init" );
    sysproc_init() ;
    STEP( "sysproc_init ok" );

#ifdef _WIN32
    HWND hWnd = /*GetForegroundWindow()*/ GetConsoleWindow();
    DWORD dwProcessId;
    GetWindowThreadProcessId( hWnd, &dwProcessId );
    if ( dwProcessId == GetCurrentProcessId() ) ShowWindow( hWnd, SW_HIDE );
#endif

    argv[0] = filename;
    STEP( "bgdrtm_entry" );
    bgdrtm_entry( argc, argv );
    STEP( "running" );

    if ( mainproc )
    {
        mainproc_running = instance_new( mainproc, NULL ) ;
        ret = instance_go_all() ;
    }

#ifdef TARGET_PSP
    sceKernelExitGame();
#endif

    bgdrtm_exit( ret );

    free( appexename        );
    free( appexepath        );
    free( appexefullpath    );
    free( appname           );

    return ret;
}

