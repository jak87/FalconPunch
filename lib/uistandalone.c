/******************************************************************************
* Copyright (C) 2011 by Jonathan Appavoo, Boston University
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h> /* for exit() */
#include <pthread.h>
#include <assert.h>
#include "uistandalone.h"
#include "net.h"
#include "maze.h"
#include "player.h"
#include "game_control.h"


/* A lot of this code comes from http://www.libsdl.org/cgi/docwiki.cgi */

static void paint_players(UI *ui);

#define MAX_NUM_PLAYERS 5 //per team

//struct {
//  Player *me; //For centering
//  Player players[2][MAX_NUM_PLAYERS];
//} ClientGameState;

#define SPRITE_H 32
#define SPRITE_W 32

#define UI_FLOOR_BMP "../lib/floor.bmp"
#define UI_REDWALL_BMP "../lib/redwall.bmp"
#define UI_GREENWALL_BMP "../lib/greenwall.bmp"
#define UI_TEAMA_BMP "../lib/teama.bmp"
#define UI_TEAMB_BMP "../lib/teamb.bmp"
#define UI_LOGO_BMP "../lib/logo.bmp"
#define UI_REDFLAG_BMP "../lib/redflag.bmp"
#define UI_GREENFLAG_BMP "../lib/greenflag.bmp"
#define UI_JACKHAMMER_BMP "../lib/shovel.bmp"

typedef enum {UI_SDLEVENT_UPDATE, UI_SDLEVENT_QUIT} UI_SDL_Event;

static inline SDL_Surface *
ui_player_img(UI *ui, int team)
{  
  return (team == 0) ? ui->sprites[TEAMA_S].img 
    : ui->sprites[TEAMB_S].img;
}

static inline sval 
pxSpriteOffSet(int team, int state)
{
  if (state == 1)
    return (team==0) ? SPRITE_W*1 : SPRITE_W*2;
  if (state == 2) 
    return (team==0) ? SPRITE_W*2 : SPRITE_W*1;
  if (state == 3) return SPRITE_W*3;
  return 0;
}

static sval
ui_uip_init(UI *ui, UI_Player **p, int id, int team)
{
  UI_Player *ui_p;
  
  ui_p = (UI_Player *)malloc(sizeof(UI_Player));
  if (!ui_p) return 0;

  ui_p->img = ui_player_img(ui, team);
  ui_p->clip.w = SPRITE_W; ui_p->clip.h = SPRITE_H; ui_p->clip.y = 0;
  ui_p->base_clip_x = id * SPRITE_W * 4;

  *p = ui_p;

  return 1;
}

/*
 * Return the pixel value at (x, y)
 * NOTE: The surface must be locked before calling this!
 */
static uint32_t 
ui_getpixel(SDL_Surface *surface, int x, int y)
{
  int bpp = surface->format->BytesPerPixel;
  /* Here p is the address to the pixel we want to retrieve */
  uint8_t *p = (uint8_t *)surface->pixels + y * surface->pitch + x * bpp;
  
  switch (bpp) {
  case 1:
    return *p;
  case 2:
    return *(uint16_t *)p;
  case 3:
    if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
      return p[0] << 16 | p[1] << 8 | p[2];
    else
      return p[0] | p[1] << 8 | p[2] << 16;
  case 4:
    return *(uint32_t *)p;
  default:
    return 0;       /* shouldn't happen, but avoids warnings */
  } // switch
}

/*
 * Set the pixel at (x, y) to the given value
 * NOTE: The surface must be locked before calling this!
 */
static void 
ui_putpixel(SDL_Surface *surface, int x, int y, uint32_t pixel)
 {
   int bpp = surface->format->BytesPerPixel;
   /* Here p is the address to the pixel we want to set */
   uint8_t *p = (uint8_t *)surface->pixels + y * surface->pitch + x * bpp;

   switch (bpp) {
   case 1:
	*p = pixel;
	break;
   case 2:
     *(uint16_t *)p = pixel;
     break;     
   case 3:
     if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
       p[0] = (pixel >> 16) & 0xff;
       p[1] = (pixel >> 8) & 0xff;
       p[2] = pixel & 0xff;
     }
     else {
       p[0] = pixel & 0xff;
       p[1] = (pixel >> 8) & 0xff;
       p[2] = (pixel >> 16) & 0xff;
     }
     break;
 
   case 4:
     *(uint32_t *)p = pixel;
     break;
 
   default:
     break;           /* shouldn't happen, but avoids warnings */
   } // switch
 }

/*draws an inscribed circle of color c, based on rasterCircle from the
 *wikipedia page for the midpoint circle algorithm
 */
static void
ui_draw_circle(SDL_Surface *s, SDL_Rect *t, uint32_t c){
  int x0 = t->x + (t->w/2);
  int y0 = t->y + (t->h/2);
  int radius = t->w/2 - 1;
  int f = 1-radius;
  int ddF_x = 1;
  int ddF_y = -2*radius;
  int x = 0;
  int y = radius;
  int i;
  SDL_LockSurface(s);
  ui_putpixel(s, x0, y0+radius, c);
  ui_putpixel(s, x0, y0-radius, c);
  ui_putpixel(s, x0+radius, y0, c);
  ui_putpixel(s, x0-radius, y0, c);

  while(x < y){
    if(f >= 0){
      y--;
      ddF_y += 2;
      f+= ddF_y;
    }
    x++;
    ddF_x += 2;
    f+= ddF_x;
    ui_putpixel(s, x0+x, y0+y, c);
    ui_putpixel(s, x0-x, y0+y, c);
    ui_putpixel(s, x0+x, y0-y, c);
    ui_putpixel(s, x0-x, y0-y, c);
    ui_putpixel(s, x0+y, y0+x, c);
    ui_putpixel(s, x0-y, y0+x, c);
    ui_putpixel(s, x0+y, y0-x, c);
    ui_putpixel(s, x0-y, y0-x, c);
  }
  SDL_UnlockSurface(s);
}

static 
sval splash(UI *ui)
{
  SDL_Rect r;
  SDL_Surface *temp;


  temp = SDL_LoadBMP(UI_LOGO_BMP);
  
  if (temp != NULL) {
    ui->sprites[LOGO_S].img = SDL_DisplayFormat(temp);
    SDL_FreeSurface(temp);
    r.h = ui->sprites[LOGO_S].img->h;
    r.w = ui->sprites[LOGO_S].img->w;
    r.x = ui->screen->w/2 - r.w/2;
    r.y = ui->screen->h/2 - r.h/2;
    //    printf("r.h=%d r.w=%d r.x=%d r.y=%d\n", r.h, r.w, r.x, r.y);
    SDL_BlitSurface(ui->sprites[LOGO_S].img, NULL, ui->screen, &r);
  } else {
    /* Map the color yellow to this display (R=0xff, G=0xFF, B=0x00)
       Note:  If the display is palettized, you must set the palette first.
    */
    r.h = 40;
    r.w = 80;
    r.x = ui->screen->w/2 - r.w/2;
    r.y = ui->screen->h/2 - r.h/2;
 
    /* Lock the screen for direct access to the pixels */
    if ( SDL_MUSTLOCK(ui->screen) ) {
      if ( SDL_LockSurface(ui->screen) < 0 ) {
	fprintf(stderr, "Can't lock screen: %s\n", SDL_GetError());
	return -1;
      }
    }
    SDL_FillRect(ui->screen, &r, ui->yellow_c);

    if ( SDL_MUSTLOCK(ui->screen) ) {
      SDL_UnlockSurface(ui->screen);
    }
  }
  /* Update just the part of the display that we've changed */
  SDL_UpdateRect(ui->screen, r.x, r.y, r.w, r.h);

  SDL_Delay(1000);
  return 1;
}


static sval
load_sprites(UI *ui) 
{
  SDL_Surface *temp;
  sval colorkey;

  /* setup sprite colorkey and turn on RLE */
  // FIXME:  Don't know why colorkey = purple_c; does not work here???
  colorkey = SDL_MapRGB(ui->screen->format, 255, 0, 255);
  
  temp = SDL_LoadBMP(UI_TEAMA_BMP);
  if (temp == NULL) { 
    fprintf(stderr, "ERROR: loading teama.bmp: %s\n", SDL_GetError()); 
    return -1;
  }
  ui->sprites[TEAMA_S].img = SDL_DisplayFormat(temp);
  SDL_FreeSurface(temp);
  SDL_SetColorKey(ui->sprites[TEAMA_S].img, SDL_SRCCOLORKEY | SDL_RLEACCEL, 
		  colorkey);

  temp = SDL_LoadBMP(UI_TEAMB_BMP);
  if (temp == NULL) { 
    fprintf(stderr, "ERROR: loading teamb.bmp: %s\n", SDL_GetError()); 
    return -1;
  }
  ui->sprites[TEAMB_S].img = SDL_DisplayFormat(temp);
  SDL_FreeSurface(temp);

  SDL_SetColorKey(ui->sprites[TEAMB_S].img, SDL_SRCCOLORKEY | SDL_RLEACCEL, 
		  colorkey);

  temp = SDL_LoadBMP(UI_FLOOR_BMP);
  if (temp == NULL) {
    fprintf(stderr, "ERROR: loading floor.bmp %s\n", SDL_GetError()); 
    return -1;
  }
  ui->sprites[FLOOR_S].img = SDL_DisplayFormat(temp);
  SDL_FreeSurface(temp);
  SDL_SetColorKey(ui->sprites[FLOOR_S].img, SDL_SRCCOLORKEY | SDL_RLEACCEL, 
		  colorkey);

  temp = SDL_LoadBMP(UI_REDWALL_BMP);
  if (temp == NULL) { 
    fprintf(stderr, "ERROR: loading redwall.bmp: %s\n", SDL_GetError());
    return -1;
  }
  ui->sprites[REDWALL_S].img = SDL_DisplayFormat(temp);
  SDL_FreeSurface(temp);
  SDL_SetColorKey(ui->sprites[REDWALL_S].img, SDL_SRCCOLORKEY | SDL_RLEACCEL, 
		  colorkey);

  temp = SDL_LoadBMP(UI_GREENWALL_BMP);
  if (temp == NULL) {
    fprintf(stderr, "ERROR: loading greenwall.bmp: %s", SDL_GetError()); 
    return -1;
  }
  ui->sprites[GREENWALL_S].img = SDL_DisplayFormat(temp);
  SDL_FreeSurface(temp);
  SDL_SetColorKey(ui->sprites[GREENWALL_S].img, SDL_SRCCOLORKEY | SDL_RLEACCEL,
		  colorkey);

  temp = SDL_LoadBMP(UI_REDFLAG_BMP);
  if (temp == NULL) {
    fprintf(stderr, "ERROR: loading redflag.bmp: %s", SDL_GetError()); 
    return -1;
  }
  ui->sprites[REDFLAG_S].img = SDL_DisplayFormat(temp);
  SDL_FreeSurface(temp);
  SDL_SetColorKey(ui->sprites[REDFLAG_S].img, SDL_SRCCOLORKEY | SDL_RLEACCEL,
		  colorkey);

  temp = SDL_LoadBMP(UI_GREENFLAG_BMP);
  if (temp == NULL) {
    fprintf(stderr, "ERROR: loading redflag.bmp: %s", SDL_GetError()); 
    return -1;
  }
  ui->sprites[GREENFLAG_S].img = SDL_DisplayFormat(temp);
  SDL_FreeSurface(temp);
  SDL_SetColorKey(ui->sprites[GREENFLAG_S].img, SDL_SRCCOLORKEY | SDL_RLEACCEL,
		  colorkey);

  temp = SDL_LoadBMP(UI_JACKHAMMER_BMP);
  if (temp == NULL) {
    fprintf(stderr, "ERROR: loading %s: %s", UI_JACKHAMMER_BMP, SDL_GetError()); 
    return -1;
  }
  ui->sprites[JACKHAMMER_S].img = SDL_DisplayFormat(temp);
  SDL_FreeSurface(temp);
  SDL_SetColorKey(ui->sprites[JACKHAMMER_S].img, SDL_SRCCOLORKEY | SDL_RLEACCEL,
		  colorkey);
  
  return 1;
}


inline static void
draw_cell(UI *ui, SPRITE_INDEX si, SDL_Rect *t, SDL_Surface *s, uint32_t c)
{
  SDL_Surface *ts=NULL;
  uint32_t tc;

  ts = ui->sprites[si].img;

  if ( ts && t->h == SPRITE_H && t->w == SPRITE_W){ 
    SDL_BlitSurface(ts, NULL, s, t);
  }
  else{
    SDL_FillRect(s, t, c);
  }
}

static void
ui_check_camera_edges(UI *ui){
  int edge;
  int max_w = Board.size * ui->tile_w;
  int max_h = Board.size * ui->tile_h;
  if(ui->camera.x < 0){
    ui->camera.x = 0;
  }
  if(ui->camera.y < 0){
    ui->camera.y = 0;
  }
  if(ui->camera.x > (edge = max_w - ui->camera.w)){
    ui->camera.x = edge;
  }
  if(ui->camera.y > (edge = max_h - ui->camera.h)){
    ui->camera.y = edge;
  }
}

static sval
ui_center_on_player(UI *ui){
  int player_x = ClientGameState.me->x * ui->tile_w;
  int player_y = ClientGameState.me->y * ui->tile_h;

  ui->camera.x = player_x - ui->camera.w / 2;
  ui->camera.y = player_y - ui->camera.h / 2;

  ui_check_camera_edges(ui);
}

sval
ui_paintmap(UI *ui) 
{

  SDL_BlitSurface(ui->fullMap, &ui->camera, ui->screen, NULL);
  paint_players(ui);

  SDL_UpdateRect(ui->screen, 0, 0, ui->screen->w, ui->screen->h);
  return 1;
}

static void ui_update_fullMap(UI *ui){
  SDL_Rect t;
  int i = 0; int j = 0;
  t.y = 0; t.x = 0; t.h = ui->tile_h; t.w = ui->tile_w; 
  SPRITE_INDEX si;
  uint32_t c;
  for (t.y=0; t.y<ui->fullMap->h && i < Board.size; t.y+=t.h) {
    for (t.x=0; t.x<ui->fullMap->w && j < Board.size; t.x+=t.w) {
      Cell * cell = Board.cells[i][j];
      switch(cell->type){
      case '#':
	if (cell->team == 1){si = REDWALL_S; c = ui->wall_teama_c;} 
	else{si = GREENWALL_S; c = ui->wall_teamb_c;}
	break;
      default:
	si = FLOOR_S;
	c = ui->isle_c;
	break;
      }
      draw_cell(ui, si, &t, ui->fullMap, c);
      j++;
    }
    i++;
    j=0;
  }
}

static sval ui_init_fullMap(UI *ui){
  //create RGB surface
  int h = Board.size * SPRITE_H;
  int w = Board.size * SPRITE_W;
  SDL_PixelFormat *fmt;
  fmt = ui->screen->format;
  ui->fullMap = SDL_CreateRGBSurface(SDL_SWSURFACE,
				     h,
				     w,
				     ui->depth,
				     fmt->Rmask, fmt->Gmask,
				     fmt->Bmask, fmt->Amask);
  if(ui->fullMap == NULL){
    fprintf(stderr, "Error Creating fullMap: %s\n", SDL_GetError());
    return -1;
  }

  ui_update_fullMap(ui);
}

static sval
ui_init_sdl(UI *ui, int32_t h, int32_t w, int32_t d)
{

  fprintf(stderr, "UI_init: Initializing SDL.\n");

  /* Initialize defaults, Video and Audio subsystems */
  if((SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==-1)) { 
    fprintf(stderr, "Could not initialize SDL: %s.\n", SDL_GetError());
    return -1;
  }

  atexit(SDL_Quit);

  fprintf(stderr, "ui_init: h=%d w=%d d=%d\n", h, w, d);

  ui->depth = d;
  ui->screen = SDL_SetVideoMode(w, h, ui->depth, SDL_SWSURFACE);
  if ( ui->screen == NULL ) {
    fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n", w, h, ui->depth, 
	    SDL_GetError());
    return -1;
  }
    
  fprintf(stderr, "UI_init: SDL initialized.\n");

  if (load_sprites(ui)<=0) return -1;

  if(ui_init_fullMap(ui) < 0){return -1;}

  ui->camera.w = w; ui->camera.h = h;

  ui->black_c      = SDL_MapRGB(ui->screen->format, 0x00, 0x00, 0x00);
  ui->white_c      = SDL_MapRGB(ui->screen->format, 0xff, 0xff, 0xff);
  ui->red_c        = SDL_MapRGB(ui->screen->format, 0xff, 0x00, 0x00);
  ui->green_c      = SDL_MapRGB(ui->screen->format, 0x00, 0xff, 0x00);
  ui->yellow_c     = SDL_MapRGB(ui->screen->format, 0xff, 0xff, 0x00);
  ui->purple_c     = SDL_MapRGB(ui->screen->format, 0xff, 0x00, 0xff);

  ui->isle_c         = ui->black_c;
  ui->wall_teama_c   = ui->red_c;
  ui->wall_teamb_c   = ui->green_c;
  ui->player_teama_c = ui->red_c;
  ui->player_teamb_c = ui->green_c;
  ui->flag_teama_c   = ui->white_c;
  ui->flag_teamb_c   = ui->white_c;
  ui->jackhammer_c   = ui->yellow_c;
  
 
  /* set keyboard repeat */
  SDL_EnableKeyRepeat(70, 70);  

  SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
  SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_IGNORE);
  SDL_EventState(SDL_MOUSEBUTTONUP, SDL_IGNORE);

  splash(ui);
  return 1;
}

static void
ui_shutdown_sdl(void)
{
  fprintf(stderr, "UI_shutdown: Quitting SDL.\n");
  SDL_Quit();
}

static sval
ui_userevent(UI *ui, SDL_UserEvent *e) 
{
  if (e->code == UI_SDLEVENT_UPDATE) return 2;
  if (e->code == UI_SDLEVENT_QUIT) return -1;
  return 0;
}

static sval
ui_process(UI *ui)
{
  SDL_Event e;
  sval rc = 1;

  while(SDL_WaitEvent(&e)) {
    switch (e.type) {
    case SDL_QUIT:
      return -1;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
      rc = ui_keypress(ui, &(e.key));
      break;
    case SDL_ACTIVEEVENT:
      break;
    case SDL_USEREVENT:
      rc = ui_userevent(ui, &(e.user));
      break;
    default:
      fprintf(stderr, "%s: e.type=%d NOT Handled\n", __func__, e.type);
    }
    if (rc==2) { ui_paintmap(ui); }
    if (rc<0) break;
  }
  return rc;
}

extern sval
ui_zoom(UI *ui, sval fac)
{
  SDL_FillRect(ui->fullMap, 0, ui->black_c);
  (ui->currentZoom = ((fac > 0) ? ui->currentZoom + 1 : ui->currentZoom-1) % 3);
  switch(ui->currentZoom){
  case 0:
    ui->tile_h = SPRITE_H;
    ui->tile_w = SPRITE_W;
    break;
  case 1:
    ui->tile_h = 16;
    ui->tile_w = 16;
    break;
  case 2:
    ui->tile_h = 8;
    ui->tile_w = 8;
    break;
  }
  ui_update_fullMap(ui);
  ui_center_on_player(ui);
  return 2;
}

extern sval
ui_pan(UI *ui, sval xdir, sval ydir)
{
  if(xdir < 0){ui->camera.x -= ui->tile_w;}
  else if(xdir > 0){ui->camera.x += ui->tile_w;}
  if(ydir < 0){ui->camera.y -= ui->tile_h;}
  else if(ydir > 0){ui->camera.y += ui->tile_h;}
  
  ui_check_camera_edges(ui);

  return 2;
}

extern sval
ui_move(UI *ui, sval xdir, sval ydir)
{
  fprintf(stderr, "%s:\n", __func__);
  return 1;
}


extern void
ui_update(UI *ui)
{
  SDL_Event event;
  
  event.type      = SDL_USEREVENT;
  event.user.code = UI_SDLEVENT_UPDATE;
  SDL_PushEvent(&event);

}


extern void
ui_quit(UI *ui)
{  
  SDL_Event event;
  fprintf(stderr, "ui_quit: stopping ui...\n");
  event.type      = SDL_USEREVENT;
  event.user.code = UI_SDLEVENT_QUIT;
  SDL_PushEvent(&event);
}

static void
init_test_struct(){
  int i, j;
  Player *p;
  srand(time(NULL));
  for(i = 0; i < 2; i++){
    for(j = 0; j < MAX_NUM_PLAYERS; j++){
      p = &ClientGameState.players[i][j];
      p->id = j;
      p->x = rand() % Board.size;
      p->y = rand() % Board.size;
      p->team = i;
      p->state = 0;
      p->uip = NULL;
    }
  }
  ClientGameState.me = &(ClientGameState.players[rand() % 2][rand() % MAX_NUM_PLAYERS]);
}

extern void
ui_main_loop(UI *ui, uval h, uval w)
{
  sval rc;
  
  assert(ui);

  ui_init_sdl(ui, h, w, 32);

//  init_test_struct();
  ui_center_on_player(ui);
  ui_paintmap(ui);
   
  
  while (1) {
    if (ui_process(ui)<0) break;
  }

  ui_shutdown_sdl();
}


extern void
ui_init(UI **ui)
{
  *ui = (UI *)malloc(sizeof(UI));
  if (ui==NULL) return;

  bzero(*ui, sizeof(UI));
  
  (*ui)->tile_h = SPRITE_H;
  (*ui)->tile_w = SPRITE_W;

}

static void
ui_player_init(UI *ui, Player *player){
  ui_uip_init(ui, &(player->uip), player->id, player->team);
}

static void
paint_players(UI *ui)
{
  SDL_Rect t;
  t.h = ui->tile_h; t.w = ui->tile_w; t.y = 0; t.x = 0;
  int start_x = ui->camera.x / t.w;
  int max_x = (ui->camera.w / t.w) + start_x;
  int start_y = ui->camera.y / t.h;
  int max_y = (ui->camera.h / t.h) + start_y;
  
  //this is just for testing to make sure im on the right track
  Player *player;
  int i, j;
  for (i = 0; i < 2; i++){
    for(j = 0; j < MAX_NUM_PLAYERS; j++){
      if((player = &ClientGameState.players[i][j])->id < 0){continue;}
      if(player->uip == NULL){ui_player_init(ui, player);}
      if((player->x >= start_x) && (player->x < max_x) &&
	 (player->y >= start_y) && (player->y < max_y)){
	t.x = (player->x - start_x) * t.w;
	t.y = (player->y - start_y) * t.h;
	fprintf(stderr, "id:%d\npx offsets x:%d y:%d\nraw x:%d y%d\n", player->id, t.x, t.y, player->x, player->y);
	if((ui->tile_h == SPRITE_H) && (ui->tile_w == SPRITE_W)){
	  player->uip->clip.x = player->uip->base_clip_x +
	    pxSpriteOffSet(player->team, player->state);
	  fprintf(stderr, "sprite clip%d\n", player->uip->clip.x);
	  SDL_BlitSurface(player->uip->img, &(player->uip->clip), ui->screen, &t);
	}
	else{
	  uint32_t c = (player->team == 0) ? ui->player_teama_c : ui->player_teamb_c;
	    ui_draw_circle(ui->screen, &t, c);
	}
      }
    }
  }
}

int
ui_left(UI *ui)
{
  //pthread_mutex_lock(&dummyPlayer.lock);
  if(Board.cells[ClientGameState.me->y][ClientGameState.me->x-1]->type != '#'){  
    ClientGameState.me->x--;
  }
  //pthread_mutex_unlock(&dummyPlayer.lock);
  ui_center_on_player(ui);
  return 2;
}

int
ui_right(UI *ui)
{
  //pthread_mutex_lock(&dummyPlayer.lock);
  if(Board.cells[ClientGameState.me->y][ClientGameState.me->x+1]->type != '#'){  
    ClientGameState.me->x++;
  }
  //pthread_mutex_unlock(&dummyPlayer.lock);
  ui_center_on_player(ui);
  return 2;
}

int
ui_down(UI *ui)
{
  //pthread_mutex_lock(&dummyPlayer.lock);
  if(Board.cells[ClientGameState.me->y+1][ClientGameState.me->x]->type != '#'){  
    ClientGameState.me->y++;
  }
  //pthread_mutex_unlock(&dummyPlayer.lock);
  ui_center_on_player(ui);
  return 2;
}

int
ui_up(UI *ui)
{
  //pthread_mutex_lock(&dummyPlayer.lock);
  if(Board.cells[ClientGameState.me->y-1][ClientGameState.me->x]->type != '#'){  
    ClientGameState.me->y--;
  }
  //pthread_mutex_unlock(&dummyPlayer.lock);
  ui_center_on_player(ui);
  return 2;
}

/*int
ui_dummy_normal(UI *ui)
{
  pthread_mutex_lock(&dummyPlayer.lock);
    dummyPlayer.state = 0;
  pthread_mutex_unlock(&dummyPlayer.lock);
  return 2;
}

int
ui_dummy_pickup_red(UI *ui)
{
  pthread_mutex_lock(&dummyPlayer.lock);
    dummyPlayer.state = 1;
  pthread_mutex_unlock(&dummyPlayer.lock);
  return 2;
  }*/

 /*int
ui_dummy_pickup_green(UI *ui)
{
  pthread_mutex_lock(&dummyPlayer.lock);
    dummyPlayer.state = 2;
  pthread_mutex_unlock(&dummyPlayer.lock);
  return 2;
  }*/


  /*int
ui_dummy_jail(UI *ui)
{
  pthread_mutex_lock(&dummyPlayer.lock);
    dummyPlayer.state = 3;
  pthread_mutex_unlock(&dummyPlayer.lock);
  return 2;
  }

int
ui_dummy_toggle_team(UI *ui)
{
  pthread_mutex_lock(&dummyPlayer.lock);
    if (dummyPlayer.uip) free(dummyPlayer.uip);
    dummyPlayer.team = (dummyPlayer.team) ? 0 : 1;
    ui_uip_init(ui, &dummyPlayer.uip, dummyPlayer.id, dummyPlayer.team);
  pthread_mutex_unlock(&dummyPlayer.lock);
  return 2;
}

int
ui_dummy_inc_id(UI *ui)
{
  pthread_mutex_lock(&dummyPlayer.lock);
    if (dummyPlayer.uip) free(dummyPlayer.uip);
    dummyPlayer.id++;
    if (dummyPlayer.id>=100) dummyPlayer.id = 0;
    ui_uip_init(ui, &dummyPlayer.uip, dummyPlayer.id, dummyPlayer.team);
  pthread_mutex_unlock(&dummyPlayer.lock);
  return 2;
  }*/

int
ui_inc_state(){
  int *s = &ClientGameState.me->state;
  *s = (*s +1) % 4;
  return 2;
}
