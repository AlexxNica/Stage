///////////////////////////////////////////////////////////////////////////
//
// File: model_gripper.c
// Author: Doug Blank, Richard Vaughan
// Date: 21 April 2005
//
// CVS info:
//  $Source: /home/tcollett/stagecvs/playerstage-cvs/code/stage/src/model_gripper.c,v $
//  $Author: rtv $
//  $Revision: 1.3 $
//
///////////////////////////////////////////////////////////////////////////


#include <sys/time.h>
#include <math.h>
#include "gui.h"

//#define DEBUG

#define STG_DEFAULT_GRIPPER_SIZEX 0.14
#define STG_DEFAULT_GRIPPER_SIZEY 0.30

#include "stage_internal.h"

void gripper_load( stg_model_t* mod )
{
  stg_gripper_config_t gconf;
  stg_model_get_config( mod, &gconf, sizeof(gconf));
  // 
  stg_model_set_config( mod, &gconf, sizeof(gconf));
}

int gripper_update( stg_model_t* mod );
int gripper_startup( stg_model_t* mod );
int gripper_shutdown( stg_model_t* mod );
void gripper_render_data(  stg_model_t* mod );
void gripper_render_cfg( stg_model_t* mod );

void gripper_generate_paddles( stg_model_t* mod, stg_gripper_config_t* cfg );

void stg_polygon_print( stg_polygon_t* poly )
{
  printf( "polygon: %d pts : ", poly->points->len );
  
  int i;
  for(i=0;i<poly->points->len;i++)
    {
      stg_point_t* pt = &g_array_index( poly->points, stg_point_t, i );
      printf( "(%.2f,%.2f) ", pt->x, pt->y );
    }
  puts("");
}

void stg_polygons_print( stg_polygon_t* polys, unsigned int count )
{
  printf( "polygon array (%d polys)\n", count );
  
  int i;
  for( i=0; i<count; i++ )
    {
      printf( "[%d] ", i ); 
      stg_polygon_print( &polys[i] );
    }
}

stg_model_t* stg_gripper_create( stg_world_t* world, 
			       stg_model_t* parent, 
			       stg_id_t id, 
			       char* token )
{
  stg_model_t* mod = 
    stg_model_create( world, parent, id, STG_MODEL_GRIPPER, token );
  
  // we don't consume any power until subscribed
  //mod->watts = 0.0; 
  
  // override the default methods
  mod->f_startup = gripper_startup;
  mod->f_shutdown = gripper_shutdown;
  mod->f_update =  gripper_update;
  mod->f_render_data = gripper_render_data;
  mod->f_render_cfg = gripper_render_cfg;
  mod->f_load = gripper_load;

  // sensible gripper defaults
  stg_geom_t geom;
  geom.pose.x = 0;//-STG_DEFAULT_GRIPPER_SIZEX/2.0;
  geom.pose.y = 0.0;
  geom.pose.a = 0.0;
  geom.size.x = STG_DEFAULT_GRIPPER_SIZEX;
  geom.size.y = STG_DEFAULT_GRIPPER_SIZEY;
  stg_model_set_geom( mod, &geom );

  // set up a gripper-specific config structure
  stg_gripper_config_t gconf;
  memset(&gconf,0,sizeof(gconf));  
  gconf.paddle_size.x = 0.6;
  gconf.paddle_size.y = 0.1;

  gconf.paddles = STG_GRIPPER_PADDLE_OPEN;
  gconf.lift = STG_GRIPPER_LIFT_DOWN;
  
  // place the break beam sensors at 1/4 and 3/4 the length of the paddle 
  gconf.inner_break_beam_inset = 3.0/4.0 * gconf.paddle_size.x;
  gconf.outer_break_beam_inset = 1.0/4.0 * gconf.paddle_size.x;
  
  stg_model_set_config( mod, &gconf, sizeof(gconf) );
  
  // sensible starting command
  stg_gripper_cmd_t cmd; 
  cmd.cmd = STG_GRIPPER_CMD_NOP;
  cmd.arg = 0;  
  stg_model_set_command( mod, &cmd, sizeof(cmd) ) ;

  // set default color
  stg_color_t col = stg_lookup_color( STG_GRIPPER_COLOR ); 
  stg_model_set_color( mod, &col );

  // install three polygons for the gripper body;
  stg_polygon_t* polys = stg_polygons_create( 3 );
  stg_model_set_polygons( mod, polys, 3 );
  //stg_polygons_destroy ?
  
  // figure out where the paddles should be
  gripper_generate_paddles( mod, &gconf );
  
  return mod;
}

void gripper_generate_paddles( stg_model_t* mod, stg_gripper_config_t* cfg )
{
  stg_point_t body[4];
  body[0].x = 0;
  body[0].y = 0;
  body[1].x = 1.0 - cfg->paddle_size.x;
  body[1].y = 0;
  body[2].x = body[1].x;
  body[2].y = 1.0;
  body[3].x = 0;
  body[3].y = 1.0;

  stg_point_t l_pad[4];
  l_pad[0].x = 1.0 - cfg->paddle_size.x;
  l_pad[0].y = cfg->paddle_position * (0.5 - cfg->paddle_size.y);
  l_pad[1].x = 1.0;
  l_pad[1].y = l_pad[0].y;
  l_pad[2].x = 1.0;
  l_pad[2].y = l_pad[0].y + cfg->paddle_size.y;
  l_pad[3].x = l_pad[0].x;
  l_pad[3].y = l_pad[2].y;

  stg_point_t r_pad[4];
  r_pad[0].x = 1.0 - cfg->paddle_size.x;
  r_pad[0].y = 1.0 - cfg->paddle_position * (0.5 - cfg->paddle_size.y);
  r_pad[1].x = 1.0;
  r_pad[1].y = r_pad[0].y;
  r_pad[2].x = 1.0;
  r_pad[2].y = r_pad[0].y - cfg->paddle_size.y;
  r_pad[3].x = r_pad[0].x;
  r_pad[3].y = r_pad[2].y;

  // move the paddle arm polygons
  size_t count=0;
  stg_polygon_t* polys = stg_model_get_polygons(mod, &count);
  stg_polygon_set_points( &polys[0], body, 4 );
  stg_polygon_set_points( &polys[1], l_pad, 4 );
  stg_polygon_set_points( &polys[2], r_pad, 4 );	  
  stg_model_set_polygons( mod, polys, 3 );
}

int gripper_update( stg_model_t* mod )
{   
  // no work to do if we're unsubscribed
  if( mod->subs < 1 )
    return 0;
    
  stg_gripper_config_t cfg;
  stg_model_get_config( mod, &cfg, sizeof(cfg));
  
  stg_geom_t geom;
  stg_model_get_geom( mod, &geom );
  
  stg_gripper_cmd_t cmd;
  stg_model_get_command( mod, &cmd, sizeof(cmd));
  
  switch( cmd.cmd )
    {
    case 0:
      //puts( "gripper NOP" );
      break;
      
    case STG_GRIPPER_CMD_CLOSE:     
      if( cfg.paddles == STG_GRIPPER_PADDLE_OPEN )
	{
	  //puts( "closing gripper paddles" );
	  cfg.paddles = STG_GRIPPER_PADDLE_CLOSING;
	}
      break;
      
    case STG_GRIPPER_CMD_OPEN:
      if( cfg.paddles == STG_GRIPPER_PADDLE_CLOSED )
	{
	  //puts( "opening gripper paddles" );      
	  cfg.paddles = STG_GRIPPER_PADDLE_OPENING;
	}
      break;
      
    case STG_GRIPPER_CMD_UP:
      if( cfg.lift == STG_GRIPPER_LIFT_DOWN )
	{
	  //puts( "raising gripper lift" );      
	  cfg.lift = STG_GRIPPER_LIFT_UPPING;
	}
      break;
      
    case STG_GRIPPER_CMD_DOWN:
      if( cfg.lift == STG_GRIPPER_LIFT_UP )
	{
	  //puts( "lowering gripper lift" );      
	  cfg.lift = STG_GRIPPER_LIFT_DOWNING;
	}      
      break;
      
    default:
      printf( "unknown gripper command %d\n",cmd.cmd );
    }
	      
  // get the sensor's pose in global coords
  stg_pose_t pz;
  memcpy( &pz, &geom.pose, sizeof(pz) ); 
  stg_model_local_to_global( mod, &pz );

  // cast the rays of the beams
  // compute beam and arm states
  
  
  // move the paddles 
  if( cfg.paddles == STG_GRIPPER_PADDLE_OPENING )
    {
      cfg.paddle_position -= 0.05;
      
      if( cfg.paddle_position < 0.0 ) // if we're fully open
	{
	  cfg.paddle_position == 0.0;
	  cfg.paddles = STG_GRIPPER_PADDLE_OPEN; // change state
	}
    }
  else if( cfg.paddles == STG_GRIPPER_PADDLE_CLOSING )
    {
      cfg.paddle_position += 0.05;
      
      if( cfg.paddle_position > 1.0 ) // if we're fully open
	{
	  cfg.paddle_position == 1.0;
	  cfg.paddles = STG_GRIPPER_PADDLE_CLOSED; // change state
	}
    }
  
  // move the lift 
  if( cfg.lift == STG_GRIPPER_LIFT_DOWNING )
    {
      cfg.lift_position -= 0.05;
      
      if( cfg.lift_position < 0.0 ) // if we're fully down
	{
	  cfg.lift_position == 0.0;
	  cfg.lift = STG_GRIPPER_LIFT_DOWN; // change state
	}
    }
  else if( cfg.lift == STG_GRIPPER_LIFT_UPPING )
    {
      cfg.lift_position += 0.05;
      
      if( cfg.lift_position > 1.0 ) // if we're fully open
	{
	  cfg.lift_position == 1.0;
	  cfg.lift = STG_GRIPPER_LIFT_UP; // change state
	}
    }
  
  // figure out where the paddles should be
  gripper_generate_paddles( mod, &cfg );
  
  // change the gripper's configuration in response to the commands
  stg_model_set_config( mod, &cfg, sizeof(cfg));
  //stg_print_gripper_config( &cfg );

  // also publish the data
  stg_gripper_data_t data;
  memcpy( &data, &cfg, sizeof(data));
  stg_model_set_data( mod, &data, sizeof(data));

  _model_update( mod );

  return 0; //ok
}

int break_beam(stg_model_t* mod, int beam) {
  // computing beam test
  stg_geom_t geom;
  stg_model_get_geom( mod, &geom );
  stg_pose_t pz;
  memcpy( &pz, &geom.pose, sizeof(pz) ); 
  stg_model_local_to_global( mod, &pz );
  double bearing = pz.a - 3.14/2.0;
  itl_t* itl = itl_create( pz.x, pz.y, bearing, 
			   1.0, //cfg.range_max, DISTANCE BETWEEN PADDLES
			   mod->world->matrix, 
			   PointToBearingRange );
  return 0;
}

void gripper_render_data(  stg_model_t* mod )
{
  //puts( "gripper render data" );

  if( mod->gui.data  )
    stg_rtk_fig_clear(mod->gui.data);
  else 
    {
      mod->gui.data = stg_rtk_fig_create( mod->world->win->canvas,
				      NULL, STG_LAYER_GRIPPERDATA );
      
      stg_rtk_fig_color_rgb32( mod->gui.data, stg_lookup_color(STG_GRIPPER_COLOR) );
    }
  
  if( mod->gui.data_bg )
    stg_rtk_fig_clear( mod->gui.data_bg );
  else // create the data background
    {
      mod->gui.data_bg = stg_rtk_fig_create( mod->world->win->canvas,
					 mod->gui.data, STG_LAYER_BACKGROUND );      
      stg_rtk_fig_color_rgb32( mod->gui.data_bg, 
			   stg_lookup_color( STG_GRIPPER_FILL_COLOR ));
    }
  
  stg_gripper_config_t cfg;
  assert( stg_model_get_config( mod, &cfg, sizeof(cfg))
	  == sizeof(stg_gripper_config_t ));
  stg_pose_t pose;
  stg_model_get_global_pose( mod, &pose );
  
  stg_geom_t geom;
  stg_model_get_geom( mod, &geom );
  
  stg_rtk_fig_origin( mod->gui.data, pose.x, pose.y, pose.a );  
  stg_rtk_fig_color_rgb32( mod->gui.data, stg_lookup_color(STG_GRIPPER_BRIGHT_COLOR) );  
  
  stg_gripper_data_t data;
  
  if( stg_model_get_data( mod, &data, sizeof(data)) < sizeof(stg_gripper_data_t) )
    return; // no data
  
  //stg_rtk_fig_rectangle( mod->gui.data, 0,0,0, geom.size.x, geom.size.y, 0 );

  // render the break beam state
  if( data.inner_break_beam )
    {
      // if beam is triggered, draw it red
      stg_rtk_fig_color_rgb32( mod->gui.data, 0xFF0000 );      
    }
  
  // different x location for each beam
  double ibbx = (data.inner_break_beam_inset) * geom.size.x;
  double obbx = (data.outer_break_beam_inset) * geom.size.x;
  
  // common y position
  double bby = 
    (1.0-data.paddle_position) * ((geom.size.y/2.0)-(geom.size.y*data.paddle_size.y));
  
  // common range
  double bbr = 
    (1.0-data.paddle_position) * (geom.size.y - (geom.size.y * data.paddle_size.y * 2.0 ));
  
  // draw the position of the break beam sensors
  stg_rtk_fig_color_rgb32( mod->gui.data, 0x00 );      
  stg_rtk_fig_rectangle( mod->gui.data, ibbx, bby, 0, 0.01, 0.01, 0 );
  stg_rtk_fig_rectangle( mod->gui.data, obbx, bby, 0, 0.01, 0.01, 0 );
  
  stg_rtk_fig_color_rgb32( mod->gui.data, stg_lookup_color(STG_GRIPPER_BRIGHT_COLOR));
  stg_rtk_fig_arrow( mod->gui.data, ibbx, bby, -M_PI/2.0, bbr, 0.01 );
  stg_rtk_fig_arrow( mod->gui.data, obbx, bby, -M_PI/2.0, bbr, 0.01 );
}

void gripper_render_cfg( stg_model_t* mod )
{ 
  if( mod->gui.cfg  )
    stg_rtk_fig_clear(mod->gui.cfg);
  else // create the figure, store it in the model and keep a local pointer
    mod->gui.cfg = stg_rtk_fig_create( mod->world->win->canvas, 
				   mod->gui.top, STG_LAYER_GRIPPERCONFIG );
  stg_rtk_fig_color_rgb32( mod->gui.cfg, stg_lookup_color( STG_GRIPPER_CFG_COLOR ));
  // get the config and make sure it's the right size
  stg_gripper_config_t cfg;
  size_t len = stg_model_get_config( mod, &cfg, sizeof(cfg));
  assert( len == sizeof(cfg) );
}

int gripper_startup( stg_model_t* mod )
{ 
  PRINT_DEBUG( "gripper startup" );
  return 0; // ok
}

int gripper_shutdown( stg_model_t* mod )
{ 
  PRINT_DEBUG( "gripper shutdown" );
    stg_model_set_data( mod, NULL, 0 );
  return 0; // ok
}

void stg_print_gripper_config( stg_gripper_config_t* cfg ) 
{
  char* pstate;
  switch( cfg->paddles )
    {
    case STG_GRIPPER_PADDLE_OPEN: pstate = "OPEN"; break;
    case STG_GRIPPER_PADDLE_CLOSED: pstate = "CLOSED"; break;
    case STG_GRIPPER_PADDLE_OPENING: pstate = "OPENING"; break;
    case STG_GRIPPER_PADDLE_CLOSING: pstate = "CLOSING"; break;
    default: pstate = "*unknown*";
    }
  
  char* lstate;
  switch( cfg->lift )
    {
    case STG_GRIPPER_LIFT_UP: lstate = "UP"; break;
    case STG_GRIPPER_LIFT_DOWN: lstate = "DOWN"; break;
    case STG_GRIPPER_LIFT_DOWNING: lstate = "DOWNING"; break;
    case STG_GRIPPER_LIFT_UPPING: lstate = "UPPING"; break;
    default: lstate = "*unknown*";
    }
  
  printf("gripper state: paddles(%s)[%.2f] lift(%s)[%.2f]\n", 
	 pstate, lstate, cfg->paddle_position, cfg->lift_position );
}
