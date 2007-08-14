

/*
 * includes
 */
/* localised global */
#include "SDL.h"

/* global */
#include <unistd.h> /* getopt */
#include <string.h> /* strdup */
#include <getopt.h> /* getopt_long */

/* local */
#include "main.h"
#include "conf.h"
#include "log.h"
#include "physics.h"
#include "opengl.h"
#include "ship.h"
#include "pilot.h"
#include "player.h"
#include "joystick.h"
#include "space.h"
#include "rng.h"
#include "ai.h"
#include "outfit.h"
#include "pack.h"
#include "weapon.h"
#include "faction.h"
#include "xml.h"


/* to get data info */
#define XML_START_ID		"Start"
#define START_DATA		"dat/start.xml"

#define CONF_FILE			"conf"
#define VERSION_FILE		"VERSION"
#define VERSION_LEN		10
#define MINIMUM_FPS		0.5
#define FONT_SIZE			12


static int quit = 0; /* for primary loop */
static unsigned int time = 0; /* used to calculate FPS and movement */
static char version[VERSION_LEN];

/* some defaults */
#define DATA_NAME_LEN	25 /* max length of data name */
char* data = NULL;
char dataname[DATA_NAME_LEN];
int show_fps = 1; /* shows fps - default yes */
int max_fps = 0;
int indjoystick = -1;
char* namjoystick = NULL;


/*
 * prototypes
 */
static void print_usage( char **argv );
static void display_fps( const double dt );
static void window_caption (void);
static void data_name (void);
/* update */
static void update_all (void);
static void render_all (void);


/*
 * usage
 */
static void print_usage( char **argv )
{
	LOG("Usage: %s [OPTION]", argv[0]);
	LOG("Options are:");
	LOG("   -f, --fullscreen      fullscreen");
	LOG("   -F n, --fps n         limit frames per second");
	LOG("   -d s, --data s        set the data file to be s");
	/*LOG("   -w n                  set width to n");
	LOG("   -h n                  set height to n");*/
	LOG("   -j n, --joystick n    use joystick n");
	LOG("   -J s, --Joystick s    use joystick whose name contains s");
	LOG("   -h, --help            display this message and exit");
	LOG("   -v, --version         print the version and exit");
}


/*
 * main
 */
int main ( int argc, char** argv )
{
	/* print the version */
	snprintf( version, VERSION_LEN, "%d.%d.%d", VMAJOR, VMINOR, VREV );
	LOG( " "APPNAME" v%s", version );

	/* initializes SDL for possible warnings */
	SDL_Init(0);

	/* input must be initialized for config to work */
	input_init(); 

	/* set the default config values */
	conf_setDefaults();

	/* Lua to parse the configuration file */
	conf_loadConfig( CONF_FILE );

	/*
	 * parse arguments
	 */
	static struct option long_options[] = {
			{ "fullscreen", no_argument, 0, 'f' },
			{ "fps", required_argument, 0, 'F' },
			{ "data", required_argument, 0, 'd' },
			{ "joystick", required_argument, 0, 'j' },
			{ "Joystick", required_argument, 0, 'J' },
			{ "help", no_argument, 0, 'h' },
			{ "version", no_argument, 0, 'v' },
			{ NULL, 0, 0, 0 } };
	int option_index = 0;
	int c = 0;
	while ((c = getopt_long(argc, argv, "fF:d:J:j:hv", long_options, &option_index)) != -1) {
		switch (c) {
			case 'f':
				gl_screen.fullscreen = 1;
				break;
			case 'F':
				max_fps = atoi(optarg);
				break;
			case 'd':
				data = strdup(optarg);
				break;
			case 'j':
				indjoystick = atoi(optarg);
				break;
			case 'J':
				namjoystick = strdup(optarg);
				break;

			case 'v':
				LOG(APPNAME": version %d.%d.%d", VMAJOR, VMINOR, VREV);
			case 'h':
				print_usage(argv);
				exit(EXIT_SUCCESS);
		}
	}


	/* check to see if data file is valid */
	if (pack_check(data)) {
		ERR("Data file '%s' not found",data);
		WARN("You should specify which data file to use with '-d'");
		WARN("See -h or --help for more information");
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
	data_name(); /* loads the data's name and friends */
	LOG(" %s", dataname);
	DEBUG();

	/* random numbers */
	rng_init();


	/*
	 * OpenGL
	 */
	if (gl_init()) { /* initializes video output */
		ERR("Initializing video output failed, exiting...");
		SDL_Quit();
		exit(EXIT_FAILURE);
	}
	window_caption();


	/*
	 * Input
	 */
	if ((indjoystick >= 0) || (namjoystick != NULL)) {
		if (joystick_init())
			WARN("Error initializing joystick input");
		if (namjoystick != NULL) { /* use the joystick name to find a joystick */
			if (joystick_use(joystick_get(namjoystick))) {
				WARN("Failure to open any joystick, falling back to default keybinds");
				input_setDefault();
			}
			free(namjoystick);
		}
		else if (indjoystick >= 0) /* use a joystick id instead */
			if (joystick_use(indjoystick)) {
				WARN("Failure to open any joystick, falling back to default keybinds");
				input_setDefault();
			}
	}

	/*
	 * Misc
	 */
	if (ai_init())
		WARN("Error initializing AI");

	gl_fontInit( NULL, NULL, FONT_SIZE ); /* initializes default font to size */
	gui_init(); /* initializes the GUI graphics */

	
	/*
	 * data loading
	 */
	factions_load();
	outfit_load();
	ships_load();
	fleet_load();
	space_load();


	/*
	 * create new player, TODO start menu
	 */
	player_new();

	
	time = SDL_GetTicks(); /* initializes the time */
	/* 
	 * main loop
	 */
	SDL_Event event;
	/* flushes the event loop since I noticed that when the joystick is loaded it
	 * creates button events that results in the player starting out acceling */
	while (SDL_PollEvent(&event));
	/* primary loop */
	while (!quit) {
		while (SDL_PollEvent(&event)) { /* event loop */
			if (event.type == SDL_QUIT) quit = 1; /* quit is handled here */

			input_handle(&event); /* handles all the events and player keybinds */
		}
		update_all();
		render_all();
	}


	/*
	 * data unloading
	 */
	weapon_exit(); /* destroys all active weapons */
	space_exit(); /* cleans up the universe itself */
	pilots_free(); /* frees the pilots, they were locked up :( */
	gui_free(); /* frees up the player's GUI */
	fleet_free();
	ships_free();
	outfit_free();
	factions_free();

	gl_freeFont(NULL);

	/*
	 * exit subsystems
	 */
	ai_exit(); /* stops the Lua AI magic */
	joystick_exit(); /* releases joystick */
	input_exit(); /* cleans up keybindings */
	gl_exit(); /* kills video output */

	exit(EXIT_SUCCESS);
}


/*
 * updates everything
 */
static double fps_dt = 1.;
static double dt = 0.; /* used also a bit in render_all */
static void update_all (void)
{
	/* dt in ms/1000 */
	dt = (double)(SDL_GetTicks() - time) / 1000.;
	time = SDL_GetTicks();

	if (dt > MINIMUM_FPS) { /* TODO needs work */
		Vector2d pos;
		vect_csetmin(&pos, 10., (double)(gl_screen.h-40));
		SDL_GL_SwapBuffers();
		return;
	}
	/* if fps is limited */
	else if ((max_fps != 0) && (dt < 1./max_fps)) {
		double delay = 1./max_fps - dt;
		SDL_Delay( delay );
		fps_dt += delay; /* makes sure it displays the proper fps */
	}

	weapons_update(dt);
	pilots_update(dt);
}


/*
 * Renders everything
 *
 * Blitting order (layers):
 *   BG | @ stars and planets
 *      | @ background particles
 *      | @ back layer weapons
 *      X
 *   N  | @ NPC ships
 *      | @ normal layer particles (above ships)
 *      | @ front layer weapons
 *      X
 *   FG | @ player
 *      | @ foreground particles
 *      | @ text and GUI
 */
static void render_all (void)
{
	glClear(GL_COLOR_BUFFER_BIT);

	/* BG */
	space_render(dt);
	planets_render();
	weapons_render(WEAPON_LAYER_BG);
	/* N */
	pilots_render();
	weapons_render(WEAPON_LAYER_FG);
	/* FG */
	player_render();
	display_fps(dt);

	SDL_GL_SwapBuffers();
}


/*
 * displays FPS on the screen
 */
static double fps = 0.;
static double fps_cur = 0.;
static void display_fps( const double dt )
{
	fps_dt += dt;
	fps_cur += 1.;
	if (fps_dt > 1.) { /* recalculate every second */
		fps = fps_cur / fps_dt;
		fps_dt = fps_cur = 0.;
	}
	Vector2d pos;
	vect_csetmin(&pos, 10., (double)(gl_screen.h-20));
	if (show_fps)
		gl_print( NULL, &pos, NULL, "%3.2f", fps );
}


/*
 * gets the data module's name
 */
static void data_name (void)
{
	uint32_t bufsize;
	char *buf;

	/*
	 * check the version
	 */
	buf = pack_readfile( DATA, VERSION_FILE, &bufsize );

	if (strncmp(buf, version, bufsize) != 0) {
		WARN("NAEV version and data module version differ!");
		WARN("NAEV is v%s, data is for v%s", version, buf );
	}
	
	free(buf);
	
	
	/*
	 * load the datafiles name
	 */
	buf = pack_readfile( DATA, START_DATA, &bufsize );

	xmlNodePtr node;
	xmlDocPtr doc = xmlParseMemory( buf, bufsize );

	node = doc->xmlChildrenNode;
	if (!xml_isNode(node,XML_START_ID)) {
		ERR("Malformed '"START_DATA"' file: missing root element '"XML_START_ID"'");
		return;
	}

	node = node->xmlChildrenNode; /* first system node */
	if (node == NULL) {
		ERR("Malformed '"START_DATA"' file: does not contain elements");
		return;
	}
	do {
		if (xml_isNode(node,"name"))
			strncpy(dataname,xml_get(node),DATA_NAME_LEN);
	} while ((node = node->next));

	xmlFreeDoc(doc);
	free(buf);
	xmlCleanupParser();
}


/*
 * sets the window caption
 */
static void window_caption (void)
{
	char tmp[DATA_NAME_LEN+10];

	snprintf(tmp,DATA_NAME_LEN+10,APPNAME" - %s",dataname);
	SDL_WM_SetCaption(tmp, NULL );
}


